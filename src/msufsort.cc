#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <execution>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <stdlib.h>
#define __builtin_bswap32(x) _byteswap_ulong(x)
#define __builtin_bswap16(x) _byteswap_ushort(x)
#endif

#include "pysubstringsearch/src/msufsort.h"

class msufsort {
    public:

    msufsort():
        inputBegin_(nullptr),
        inputEnd_(nullptr),
        suffixArrayBegin_(nullptr),
        suffixArrayEnd_(nullptr),
        inverseSuffixArrayBegin_(nullptr),
        frontBucketOffset_(),
        backBucketOffset_(
            new int32_t * [0x10000] {}
        ),
        aCount_(),
        bCount_() {}

    ~msufsort() {}

    void calculate_suffix_array(
        rust::Slice<const uint8_t> input,
        rust::Slice<int32_t> output
    ) {
        inputBegin_ = input.data();
        inputEnd_ = input.data() + input.size() - 1;

        suffixArrayBegin_ = output.data();
        suffixArrayEnd_ = output.data() + input.size() + 1;
        inverseSuffixArrayBegin_ = (suffixArrayBegin_ + ((input.size() + 1) >> 1));

        first_stage_its();
        second_stage_its();
    }

    private:
    static int32_t constexpr IS_INDUCED_SORT = 0x40000000;
    static int32_t constexpr IS_TANDEM_REPEAT_LENGTH = 0x80000000;
    static int32_t constexpr ISA_FLAG_MASK = IS_INDUCED_SORT | IS_TANDEM_REPEAT_LENGTH;
    static int32_t constexpr ISA_INDEX_MASK = ~ISA_FLAG_MASK;

    static int32_t constexpr PRECEDING_SUFFIX_IS_TYPE_A_FLAG = 0x80000000;
    static int32_t constexpr MARK_ISA_WHEN_SORTED = 0x40000000;
    static int32_t constexpr SA_INDEX_MASK = ~(PRECEDING_SUFFIX_IS_TYPE_A_FLAG | MARK_ISA_WHEN_SORTED);
    static int32_t constexpr SUFFIX_IS_UNSORTED_B_TYPE = SA_INDEX_MASK;

    static constexpr int32_t INSERTION_SORT_THRESHOLD = 16;
    static int32_t constexpr MIN_MATCH_LENGTH_FOR_TANDEM_REPEATS = (2 + sizeof(uint32_t) + sizeof(uint32_t));

    enum suffix_type {
        a,
        b,
        bStar
    };

    struct tandem_repeat_info {
        tandem_repeat_info(
            int32_t * partitionBegin,
            int32_t * partitionEnd,
            int32_t numTerminators,
            int32_t tandemRepeatLength
        ):
            partitionBegin_(partitionBegin),
            partitionEnd_(partitionEnd),
            numTerminators_(numTerminators),
            tandemRepeatLength_(tandemRepeatLength) {}

        int32_t * partitionBegin_;
        int32_t * partitionEnd_;
        int32_t numTerminators_;
        int32_t tandemRepeatLength_;
    };

    inline auto get_value(
        uint8_t const * inputCurrent,
        int32_t index
    ) const -> uint32_t {
        return __builtin_bswap32(*(uint32_t const *)(inputCurrent + (index & SA_INDEX_MASK)));
    }

    inline auto get_suffix_type(
        uint8_t const * suffix
    ) -> suffix_type {
        if((suffix + 1) >= inputEnd_)
            return a;
        if(suffix[0] >= suffix[1]) {
            auto p = suffix + 1;
            while((p < inputEnd_) && (*p == suffix[0]))
                ++p;
            if((p == inputEnd_) || (suffix[0] > p[0]))
                return a;
            return b;
        }
        auto p = suffix + 2;
        while((p < inputEnd_) && (*p == suffix[1]))
            ++p;
        if((p == inputEnd_) || (suffix[1] > p[0]))
            return bStar;
        return b;
    }

    inline bool compare_suffixes(
        // optimized compare_suffixes for when two suffixes have long common match lengths
        uint8_t const * inputBegin,
        int32_t indexA,
        int32_t indexB
    ) const {
        return std::memcmp(
            inputBegin + (indexA & SA_INDEX_MASK),
            inputBegin + (indexB & SA_INDEX_MASK),
            std::min(
                inputEnd_ - (inputBegin + (indexA & SA_INDEX_MASK)),
                inputEnd_ - (inputBegin + (indexB & SA_INDEX_MASK))
            )
        );
    }

    void multikey_insertion_sort(
        // private:
        // sorts the suffixes by insertion sort
        int32_t * partitionBegin,
        int32_t * partitionEnd,
        int32_t currentMatchLength,
        uint32_t startingPattern,
        std::array<uint32_t, 2> endingPattern,
        std::vector<tandem_repeat_info> & tandemRepeatStack
    ) {
        int32_t partitionSize = (int32_t)std::distance(partitionBegin, partitionEnd);
        if(partitionSize < 2)
            return;
        struct partition_info {
            int32_t currentMatchLength_;
            int32_t size_;
            uint32_t startingPattern_;
            uint32_t endingPattern_;
            bool hasPotentialTandemRepeats_;
        };
        partition_info stack[INSERTION_SORT_THRESHOLD];
        stack[0] = {currentMatchLength, partitionSize, startingPattern, endingPattern[0], false};
        auto stackTop = stack + 1;

        while(stackTop-- != stack) {
            auto currentMatchLength = stackTop->currentMatchLength_;
            auto size = stackTop->size_;
            endingPattern[0] = stackTop->endingPattern_;
            auto hasPotentialTandemRepeats = stackTop->hasPotentialTandemRepeats_;
            startingPattern = stackTop->startingPattern_;

            if(size <= 2) {
                if((size == 2) && (compare_suffixes(inputBegin_ + currentMatchLength, partitionBegin[0], partitionBegin[1])))
                    std::swap(partitionBegin[0], partitionBegin[1]);
                partitionBegin += size;
            } else {
                if(currentMatchLength >= MIN_MATCH_LENGTH_FOR_TANDEM_REPEATS) {
                    if(hasPotentialTandemRepeats) {
                        auto tandemRepeatCount = partition_tandem_repeats(partitionBegin, partitionBegin + size, currentMatchLength, tandemRepeatStack);
                        size -= tandemRepeatCount;
                        partitionBegin += tandemRepeatCount;
                        if(size == 0)
                            continue;
                    }
                }
                uint32_t value[INSERTION_SORT_THRESHOLD];
                value[0] = get_value(inputBegin_ + currentMatchLength, partitionBegin[0]);
                for(int32_t i = 1; i < size; ++i) {
                    auto currentIndex = partitionBegin[i];
                    uint32_t currentValue = get_value(inputBegin_ + currentMatchLength, partitionBegin[i]);
                    auto j = i;
                    while((j > 0) && (value[j - 1] > currentValue)) {
                        value[j] = value[j - 1];
                        partitionBegin[j] = partitionBegin[j - 1];
                        --j;
                    }
                    value[j] = currentValue;
                    partitionBegin[j] = currentIndex;
                }

                auto i = (int32_t)size - 1;
                auto nextMatchLength = currentMatchLength + (int32_t)sizeof(uint32_t);
                while(i >= 0) {
                    int32_t start = i--;
                    auto startValue = value[start];
                    while((i >= 0) && (value[i] == startValue))
                        --i;
                    auto partitionSize = (start - i);
                    auto potentialTandemRepeats = has_potential_tandem_repeats(startingPattern, {endingPattern[0], startValue});
                    if(nextMatchLength == (2 + sizeof(uint32_t)))
                        startingPattern = get_value(inputBegin_, *partitionBegin);
                    *stackTop++ = partition_info{nextMatchLength, partitionSize, startingPattern, startValue, potentialTandemRepeats};
                }
            }
        }
    }

    size_t partition_tandem_repeats(
        // private:
        // the tandem repeat sort.  determines if the suffixes provided are tandem repeats
        // of other suffixes from within the same group.  If so, sorts the non tandem
        // repeat suffixes and then induces the sorted order of the suffixes which are
        // tandem repeats.
        int32_t * partitionBegin,
        int32_t * partitionEnd,
        int32_t currentMatchLength,
        std::vector<tandem_repeat_info> & tandemRepeatStack
    ) {
        auto parititionSize = std::distance(partitionBegin, partitionEnd);
        std::sort(std::execution::par_unseq, partitionBegin, partitionEnd, [](int32_t a, int32_t b) -> bool {
            return ((a & SA_INDEX_MASK) < (b & SA_INDEX_MASK));
        });
        int32_t tandemRepeatLength = 0;
        auto const halfCurrentMatchLength = (currentMatchLength >> 1);

        // determine if there are tandem repeats and, if so, what the tandem repeat length is.
        auto previousSuffixIndex = (partitionBegin[0] & SA_INDEX_MASK);
        for(auto cur = partitionBegin + 1; ((tandemRepeatLength == 0) && (cur < partitionEnd)); ++cur) {
            auto currentSuffixIndex = (*cur & SA_INDEX_MASK);
            if((previousSuffixIndex + halfCurrentMatchLength) >= currentSuffixIndex)
                tandemRepeatLength = (currentSuffixIndex - previousSuffixIndex);
            previousSuffixIndex = currentSuffixIndex;
        }
        if(tandemRepeatLength == 0)
            return 0; // no tandem repeats were found
        // tandem repeats detected.
        int32_t * terminatorsEnd = partitionEnd - 1;
        previousSuffixIndex = (partitionEnd[-1] & SA_INDEX_MASK);
        for(auto cur = partitionEnd - 2; cur >= partitionBegin; --cur) {
            auto currentSuffixIndex = (*cur & SA_INDEX_MASK);
            if((previousSuffixIndex - currentSuffixIndex) == tandemRepeatLength)
                std::swap(*terminatorsEnd--, *cur); // suffix is a tandem repeat
            previousSuffixIndex = currentSuffixIndex;
        }
        auto numTerminators = (std::distance(partitionBegin, terminatorsEnd) + 1);
        std::reverse(partitionBegin, partitionEnd);
        tandemRepeatStack.push_back(tandem_repeat_info(partitionBegin, partitionEnd, (int32_t)numTerminators, tandemRepeatLength));
        return (parititionSize - numTerminators);
    }

    void count_suffixes(
        uint8_t const * begin,
        uint8_t const * end,
        std::array<int32_t *, 4> count
    ) {
        if(begin < end)
            return;
        uint32_t state = 0;
        switch(get_suffix_type(begin)) {
            case suffix_type::a:
                state = 1;
                break;
            case suffix_type::b:
                state = 0;
                break;
            case suffix_type::bStar:
                state = 2;
                break;
        }
        auto current = begin;
        while(true) {
            ++count[state & 0x03][__builtin_bswap16(*(uint16_t const *)current)];
            if(--current < end)
                break;
            state <<= ((current[0] != current[1]) | ((state & 0x01) == 0));
            state |= (current[0] > current[1]);
        }
    }

    inline bool has_potential_tandem_repeats(
        uint32_t startingPattern,
        std::array<uint32_t, 2> endingPattern
    ) const {
        int8_t const * end = (int8_t const *)endingPattern.data();
        int8_t const * begin = end + sizeof(uint32_t);
        while(begin > end)
            if(*(uint32_t const *)--begin == *(uint32_t *)&startingPattern)
                return true;
        return false;
    }

    void complete_tandem_repeats(
        std::vector<tandem_repeat_info> & tandemRepeatStack
    ) {
        while(!tandemRepeatStack.empty()) {
            tandem_repeat_info tandemRepeat = tandemRepeatStack.back();
            tandemRepeatStack.pop_back();
            complete_tandem_repeat(tandemRepeat.partitionBegin_, tandemRepeat.partitionEnd_, tandemRepeat.numTerminators_, tandemRepeat.tandemRepeatLength_);
        }
    }

    inline void complete_tandem_repeat(
        int32_t * partitionBegin,
        int32_t * partitionEnd,
        int32_t numTerminators,
        int32_t tandemRepeatLength
    ) {
        int32_t * terminatorsBegin = partitionEnd - numTerminators;
        for(auto cur = terminatorsBegin - 1; cur >= partitionBegin; --cur) {
            auto currentSuffixIndex = (*cur & SA_INDEX_MASK);
            inverseSuffixArrayBegin_[currentSuffixIndex >> 1] = (tandemRepeatLength | IS_TANDEM_REPEAT_LENGTH);
        }
        // now use sorted order of terminators to determine sorted order of repeats.
        // figure out how many terminators sort before the repeat and how
        // many sort after the repeat.  put them on left and right extremes of the array.
        int32_t m = 0;
        int32_t a = 0;
        int32_t b = numTerminators - 1;
        int32_t numTypeA = 0;
        while(a <= b) {
            m = (a + b) >> 1;
            if(!compare_suffixes(inputBegin_, terminatorsBegin[m], terminatorsBegin[m] + tandemRepeatLength)) {
                numTypeA = m;
                b = m - 1;
            } else {
                numTypeA = m + 1;
                a = m + 1;
            }
        }
        if(numTypeA > numTerminators)
            numTypeA = numTerminators;
        int32_t numTypeB = (numTerminators - numTypeA);

        for(int32_t i = 0; i < numTypeA; ++i)
            partitionBegin[i] = terminatorsBegin[i];

        // type A repeats
        auto current = partitionBegin;
        auto currentEnd = current + numTypeA;
        auto next = currentEnd;
        while(current != currentEnd) {
            while(current != currentEnd) {
                auto index = (*current++ & SA_INDEX_MASK);
                if(index >= tandemRepeatLength) {
                    auto potentialTandemRepeatIndex = index - tandemRepeatLength;
                    auto isaValue = inverseSuffixArrayBegin_[potentialTandemRepeatIndex >> 1];
                    if((isaValue & IS_TANDEM_REPEAT_LENGTH) && ((isaValue & ISA_INDEX_MASK) == tandemRepeatLength)) {
                        auto flag = ((potentialTandemRepeatIndex > 0) && (inputBegin_[potentialTandemRepeatIndex - 1] <= inputBegin_[potentialTandemRepeatIndex])) ? 0 : PRECEDING_SUFFIX_IS_TYPE_A_FLAG;
                        *(next) = (potentialTandemRepeatIndex | flag);
                        ++next;
                    }
                }
            }
            currentEnd = next;
        }
        // type B repeats
        current = partitionEnd - 1;
        currentEnd = current - numTypeB;
        next = currentEnd;
        while(current != currentEnd) {
            while(current != currentEnd) {
                auto index = (*current-- & SA_INDEX_MASK);
                if(index >= tandemRepeatLength) {
                    auto potentialTandemRepeatIndex = index - tandemRepeatLength;
                    auto isaValue = inverseSuffixArrayBegin_[potentialTandemRepeatIndex >> 1];
                    if((isaValue & IS_TANDEM_REPEAT_LENGTH) && ((isaValue & ISA_INDEX_MASK) == tandemRepeatLength)) {
                        auto flag = ((potentialTandemRepeatIndex > 0) && (inputBegin_[potentialTandemRepeatIndex - 1] <= inputBegin_[potentialTandemRepeatIndex])) ? 0 : PRECEDING_SUFFIX_IS_TYPE_A_FLAG;
                        *(next) = (potentialTandemRepeatIndex | flag);
                        --next;
                    }
                }
            }
            currentEnd = next;
        }
    }

    auto multikey_quicksort(
        // private:
        // multi key quicksort on the input data provided
        int32_t * suffixArrayBegin,
        int32_t * suffixArrayEnd,
        size_t currentMatchLength,
        uint32_t startingPattern,
        std::array<uint32_t, 2> endingPattern,
        std::vector<tandem_repeat_info> & tandemRepeatStack
    ) -> int32_t * {
        std::vector<stack_frame> stack;
        stack.reserve((1 << 10) * 32);
        stack.push_back({suffixArrayBegin, suffixArrayEnd, currentMatchLength, startingPattern, endingPattern, tandemRepeatStack});

        while (!stack.empty())
        {
            auto & s = stack.back();
            suffixArrayBegin = s.suffixArrayBegin;
            suffixArrayEnd = s.suffixArrayEnd;
            currentMatchLength = s.currentMatchLength;
            startingPattern = s.startingPattern;
            endingPattern = s.endingPattern;
            tandemRepeatStack = s.tandemRepeatStack;
            stack.pop_back();

            uint64_t partitionSize = std::distance(suffixArrayBegin, suffixArrayEnd);
            if (partitionSize < 2)
                continue;

            if (currentMatchLength >= MIN_MATCH_LENGTH_FOR_TANDEM_REPEATS)
            {
                if (currentMatchLength == MIN_MATCH_LENGTH_FOR_TANDEM_REPEATS)
                    startingPattern = get_value(inputBegin_, *suffixArrayBegin);
                if ((partitionSize > 1) && (has_potential_tandem_repeats(startingPattern, endingPattern)))
                    suffixArrayBegin += partition_tandem_repeats(suffixArrayBegin, suffixArrayEnd, currentMatchLength, tandemRepeatStack);
                partitionSize = std::distance(suffixArrayBegin, suffixArrayEnd);
            }

            if (partitionSize < INSERTION_SORT_THRESHOLD)
            {
                multikey_insertion_sort(suffixArrayBegin, suffixArrayEnd, currentMatchLength, startingPattern, endingPattern, tandemRepeatStack);
                continue;
            }

            // select three pivots
            auto offsetInputBegin = inputBegin_ + currentMatchLength;
            auto oneSixthOfPartitionSize = (partitionSize / 6);
            auto pivotCandidate1 = suffixArrayBegin + oneSixthOfPartitionSize;
            auto pivotCandidate2 = pivotCandidate1 + oneSixthOfPartitionSize;
            auto pivotCandidate3 = pivotCandidate2 + oneSixthOfPartitionSize;
            auto pivotCandidate4 = pivotCandidate3 + oneSixthOfPartitionSize;
            auto pivotCandidate5 = pivotCandidate4 + oneSixthOfPartitionSize;
            auto pivotCandidateValue1 = get_value(offsetInputBegin, *pivotCandidate1);
            auto pivotCandidateValue2 = get_value(offsetInputBegin, *pivotCandidate2);
            auto pivotCandidateValue3 = get_value(offsetInputBegin, *pivotCandidate3);
            auto pivotCandidateValue4 = get_value(offsetInputBegin, *pivotCandidate4);
            auto pivotCandidateValue5 = get_value(offsetInputBegin, *pivotCandidate5);
            if (pivotCandidateValue1 > pivotCandidateValue2)
                std::swap(*pivotCandidate1, *pivotCandidate2), std::swap(pivotCandidateValue1, pivotCandidateValue2);
            if (pivotCandidateValue4 > pivotCandidateValue5)
                std::swap(*pivotCandidate4, *pivotCandidate5), std::swap(pivotCandidateValue4, pivotCandidateValue5);
            if (pivotCandidateValue1 > pivotCandidateValue3)
                std::swap(*pivotCandidate1, *pivotCandidate3), std::swap(pivotCandidateValue1, pivotCandidateValue3);
            if (pivotCandidateValue2 > pivotCandidateValue3)
                std::swap(*pivotCandidate2, *pivotCandidate3), std::swap(pivotCandidateValue2, pivotCandidateValue3);
            if (pivotCandidateValue1 > pivotCandidateValue4)
                std::swap(*pivotCandidate1, *pivotCandidate4), std::swap(pivotCandidateValue1, pivotCandidateValue4);
            if (pivotCandidateValue3 > pivotCandidateValue4)
                std::swap(*pivotCandidate3, *pivotCandidate4), std::swap(pivotCandidateValue3, pivotCandidateValue4);
            if (pivotCandidateValue2 > pivotCandidateValue5)
                std::swap(*pivotCandidate2, *pivotCandidate5), std::swap(pivotCandidateValue2, pivotCandidateValue5);
            if (pivotCandidateValue2 > pivotCandidateValue3)
                std::swap(*pivotCandidate2, *pivotCandidate3), std::swap(pivotCandidateValue2, pivotCandidateValue3);
            if (pivotCandidateValue4 > pivotCandidateValue5)
                std::swap(*pivotCandidate4, *pivotCandidate5), std::swap(pivotCandidateValue4, pivotCandidateValue5);
            auto pivot1 = pivotCandidateValue1;
            auto pivot2 = pivotCandidateValue3;
            auto pivot3 = pivotCandidateValue5;

            // partition seven ways
            auto curSuffix = suffixArrayBegin;
            auto beginPivot1 = suffixArrayBegin;
            auto endPivot1 = suffixArrayBegin;
            auto beginPivot2 = suffixArrayBegin;
            auto endPivot2 = suffixArrayEnd - 1;
            auto beginPivot3 = endPivot2;
            auto endPivot3 = endPivot2;

            std::swap(*curSuffix++, *pivotCandidate1);
            beginPivot2 += (pivot1 != pivot2);
            endPivot1 += (pivot1 != pivot2);
            std::swap(*curSuffix++, *pivotCandidate3);
            if (pivot2 != pivot3)
            {
                std::swap(*endPivot2--, *pivotCandidate5);
                --beginPivot3;
            }
            auto currentValue = get_value(offsetInputBegin, *curSuffix);
            auto nextValue = get_value(offsetInputBegin, curSuffix[1]);
            auto nextDValue = get_value(offsetInputBegin, *endPivot2);

            while (curSuffix <= endPivot2)
            {
                if (currentValue <= pivot2)
                {
                    auto temp = nextValue;
                    nextValue = get_value(offsetInputBegin, curSuffix[2]);
                    if (currentValue < pivot2)
                    {
                        std::swap(*beginPivot2, *curSuffix);
                        if (currentValue <= pivot1)
                        {
                            if (currentValue < pivot1)
                                std::swap(*beginPivot1++, *beginPivot2);
                            std::swap(*endPivot1++, *beginPivot2);
                        }
                        ++beginPivot2;
                    }
                    ++curSuffix;
                    currentValue = temp;
                }
                else
                {
                    auto nextValue = get_value(offsetInputBegin, endPivot2[-1]);
                    std::swap(*endPivot2, *curSuffix);
                    if (currentValue >= pivot3)
                    {
                        if (currentValue > pivot3)
                            std::swap(*endPivot2, *endPivot3--);
                        std::swap(*endPivot2, *beginPivot3--);
                    }
                    --endPivot2;
                    currentValue = nextDValue;
                    nextDValue = nextValue;
                }
            }
            if (++endPivot3 != suffixArrayEnd)
                stack.push_back({endPivot3, suffixArrayEnd, currentMatchLength, startingPattern, endingPattern, tandemRepeatStack});
            if (++beginPivot3 != endPivot3)
                stack.push_back({beginPivot3, endPivot3, (currentMatchLength + sizeof(uint32_t)), startingPattern, {endingPattern[1], pivot3}, tandemRepeatStack});
            if (++endPivot2 != beginPivot3)
                stack.push_back({endPivot2, beginPivot3, currentMatchLength, startingPattern, endingPattern, tandemRepeatStack});
            if (beginPivot2 != endPivot2)
                stack.push_back({beginPivot2, endPivot2, (currentMatchLength + sizeof(uint32_t)), startingPattern, {endingPattern[1], pivot2}, tandemRepeatStack});
            if (endPivot1 != beginPivot2)
                stack.push_back({endPivot1, beginPivot2, currentMatchLength, startingPattern, endingPattern, tandemRepeatStack});
            if (beginPivot1 != endPivot1)
                stack.push_back({beginPivot1, endPivot1, (currentMatchLength + sizeof(uint32_t)), startingPattern, {endingPattern[1], pivot1}, tandemRepeatStack});
            if (suffixArrayBegin != beginPivot1)
                stack.push_back({suffixArrayBegin, beginPivot1, currentMatchLength, startingPattern, endingPattern, tandemRepeatStack});
        }
        return suffixArrayEnd;
    }

    void second_stage_its_right_to_left_pass_multi_threaded(
        // private:
        // induce sorted position of B suffixes from sorted B* suffixes
        // This is the first half of the second stage of the ITS ... the 'right to left' pass
    ) {
        auto numThreads = std::min((int32_t)std::thread::hardware_concurrency(), 2);

        auto max_cache_size = (1 << 12);
        struct entry_type {
            uint8_t precedingSuffix_;
            int32_t precedingSuffixIndex_;
        };
        auto cache = std::vector<std::vector<entry_type>>(numThreads);
        for(auto i = 0; i < numThreads; ++i)
            cache[i] = std::vector<entry_type>(max_cache_size);

        auto numSuffixes = std::vector<int32_t>(numThreads);

        auto sCount = std::vector<std::vector<int32_t>>(numThreads);
        for(auto i = 0; i < numThreads; ++i)
            sCount[i] = std::vector<int32_t>(0x100);

        auto dest = std::vector<std::vector<int32_t *>>(numThreads);
        for(auto i = 0; i < numThreads; ++i)
            dest[i] = std::vector<int32_t *>(0x100);

        auto currentSuffix = suffixArrayBegin_ + std::distance(inputBegin_, inputEnd_);
        for(auto symbol = 0xff; symbol >= 0; --symbol) {
            auto backBucketOffset = &backBucketOffset_[symbol << 8];
            auto endSuffix = currentSuffix - bCount_[symbol];

            while(currentSuffix > endSuffix) {
                // determine how many B/B* suffixes are safe to process during this pass
                auto maxEnd = currentSuffix - (max_cache_size * numThreads);
                if(maxEnd < suffixArrayBegin_)
                    maxEnd = suffixArrayBegin_;
                if(maxEnd < endSuffix)
                    maxEnd = endSuffix;
                auto temp = currentSuffix;
                while((temp > maxEnd) && (*temp != SUFFIX_IS_UNSORTED_B_TYPE))
                    --temp;
                auto totalSuffixesPerThread = ((std::distance(temp, currentSuffix) + numThreads - 1) / numThreads);

                futures.clear();
                for(auto threadId = 0; threadId < numThreads; ++threadId) {
                    numSuffixes[threadId] = 0;
                    auto endForThisThread = currentSuffix - totalSuffixesPerThread;
                    if(endForThisThread < temp)
                        endForThisThread = temp;
                    futures.push_back(
                        std::async(
                            [](
                                uint8_t const * inputBegin,
                                int32_t * begin,
                                int32_t * end,
                                entry_type * cache,
                                int32_t & numSuffixes,
                                int32_t * suffixCount) {
                                auto curCache = cache;
                                ++begin;
                                uint8_t currentPrecedingSymbol = 0;
                                int32_t currentPrecedingSymbolCount = 0;
                                while(--begin > end) {
                                    if((*begin & PRECEDING_SUFFIX_IS_TYPE_A_FLAG) == 0) {
                                        int32_t precedingSuffixIndex = ((*begin & SA_INDEX_MASK) - 1);
                                        auto precedingSuffix = (inputBegin + precedingSuffixIndex);
                                        auto precedingSymbol = precedingSuffix[0];
                                        int32_t flag = ((precedingSuffixIndex > 0) && (precedingSuffix[-1] <= precedingSymbol)) ? 0 : PRECEDING_SUFFIX_IS_TYPE_A_FLAG;
                                        *curCache++ = {precedingSymbol, precedingSuffixIndex | flag};
                                        if(precedingSymbol != currentPrecedingSymbol) {
                                            suffixCount[currentPrecedingSymbol] += currentPrecedingSymbolCount;
                                            currentPrecedingSymbol = precedingSymbol;
                                            currentPrecedingSymbolCount = 0;
                                        }
                                        ++currentPrecedingSymbolCount;
                                    }
                                }
                                suffixCount[currentPrecedingSymbol] += currentPrecedingSymbolCount;
                                numSuffixes = std::distance(cache, curCache);
                            },
                            inputBegin_,
                            currentSuffix,
                            endForThisThread,
                            cache[threadId].data(),
                            std::ref(numSuffixes[threadId]),
                            sCount[threadId].data()));
                    currentSuffix = endForThisThread;
                }
                for (auto & future: futures) {
                    future.wait();
                }

                futures.clear();
                for(auto threadId = 0, begin = 0, numSymbolsPerThread = ((0x100 + numThreads - 1) / numThreads); threadId < numThreads; ++threadId) {
                    auto end = begin + numSymbolsPerThread;
                    if(end > 0x100)
                        end = 0x100;
                    futures.push_back(
                        std::async(
                            [&dest, &backBucketOffset, &sCount, numThreads](
                                int32_t begin,
                                int32_t end) {
                                for(auto threadId = 0; threadId < numThreads; ++threadId)
                                    for(auto symbol = begin; symbol < end; ++symbol) {
                                        dest[threadId][symbol] = backBucketOffset[symbol];
                                        backBucketOffset[symbol] -= sCount[threadId][symbol];
                                        sCount[threadId][symbol] = 0;
                                    }
                            },
                            begin,
                            end));
                    begin = end;
                }
                for(auto & future: futures) {
                    future.wait();
                }

                futures.clear();
                for(auto threadId = 0; threadId < numThreads; ++threadId)
                    futures.push_back(
                        std::async(
                            [&](
                                int32_t * dest[0x100],
                                entry_type const * begin,
                                entry_type const * end) {
                                --begin;
                                while(++begin < end)
                                    *(--dest[begin->precedingSuffix_]) = begin->precedingSuffixIndex_;
                            },
                            dest[threadId].data(),
                            cache[threadId].data(),
                            cache[threadId].data() + numSuffixes[threadId]));
                for(auto & future: futures) {
                    future.wait();
                }
            }
            currentSuffix -= aCount_[symbol];
        }
    }

    void second_stage_its_left_to_right_pass_multi_threaded(
        // private:
        // induce sorted position of A suffixes from sorted B suffixes
        // This is the second half of the second stage of the ITS ... the 'left to right' pass
    ) {
        auto numThreads = std::min((int32_t)std::thread::hardware_concurrency(), 2);
        auto currentSuffix = suffixArrayBegin_;

        auto max_cache_size = (1 << 12);
        struct entry_type {
            uint8_t precedingSuffix_;
            int32_t precedingSuffixIndex_;
        };
        auto cache = std::vector<std::vector<entry_type>>(numThreads);
        for(auto i = 0; i < numThreads; ++i)
            cache[i] = std::vector<entry_type>(max_cache_size);

        auto numSuffixes = std::vector<int32_t>(numThreads);

        auto sCount = std::vector<std::vector<int32_t>>(numThreads);
        for(auto i = 0; i < numThreads; ++i)
            sCount[i] = std::vector<int32_t>(0x100);

        auto dest = std::vector<std::vector<int32_t *>>(numThreads);
        for(auto i = 0; i < numThreads; ++i)
            dest[i] = std::vector<int32_t *>(0x100);

        while(currentSuffix < suffixArrayEnd_) {
            // calculate current 'safe' suffixes to process
            while((currentSuffix < suffixArrayEnd_) && (!(*currentSuffix & PRECEDING_SUFFIX_IS_TYPE_A_FLAG)))
                ++currentSuffix;
            if(currentSuffix >= suffixArrayEnd_)
                break;

            auto begin = currentSuffix;
            auto maxEnd = begin + (max_cache_size * numThreads);
            if(maxEnd > suffixArrayEnd_)
                maxEnd = suffixArrayEnd_;
            currentSuffix += (currentSuffix != maxEnd);
            while((currentSuffix != maxEnd) && (*currentSuffix != (int32_t)0x80000000))
                ++currentSuffix;
            auto end = currentSuffix;
            auto totalSuffixes = std::distance(begin, end);
            auto totalSuffixesPerThread = ((totalSuffixes + numThreads - 1) / numThreads);

            futures.clear();
            for(auto threadId = 0; threadId < numThreads; ++threadId) {
                numSuffixes[threadId] = 0;
                auto endForThisThread = begin + totalSuffixesPerThread;
                if(endForThisThread > end)
                    endForThisThread = end;
                futures.push_back(
                    std::async(
                        [](
                            uint8_t const * inputBegin,
                            int32_t * begin,
                            int32_t * end,
                            entry_type * cache,
                            int32_t & numSuffixes,
                            int32_t * suffixCount) {
                            auto current = begin;
                            auto curCache = cache;
                            --current;
                            uint8_t currentPrecedingSymbol = 0;
                            int32_t currentPrecedingSymbolCount = 0;
                            while(++current != end) {
                                auto currentSuffixIndex = *current;
                                if(currentSuffixIndex & PRECEDING_SUFFIX_IS_TYPE_A_FLAG) {
                                    currentSuffixIndex &= SA_INDEX_MASK;
                                    if(currentSuffixIndex != 0) {
                                        int32_t precedingSuffixIndex = (currentSuffixIndex - 1);
                                        auto precedingSuffix = (inputBegin + precedingSuffixIndex);
                                        auto precedingSymbol = precedingSuffix[0];
                                        int32_t flag = ((precedingSuffixIndex > 0) && (precedingSuffix[-1] >= precedingSymbol)) ? PRECEDING_SUFFIX_IS_TYPE_A_FLAG : 0;
                                        *curCache++ = {precedingSymbol, precedingSuffixIndex | flag};
                                        if(precedingSymbol != currentPrecedingSymbol) {
                                            suffixCount[currentPrecedingSymbol] += currentPrecedingSymbolCount;
                                            currentPrecedingSymbol = precedingSymbol;
                                            currentPrecedingSymbolCount = 0;
                                        }
                                        ++currentPrecedingSymbolCount;
                                    }
                                    *current = currentSuffixIndex;
                                }
                            }
                            suffixCount[currentPrecedingSymbol] += currentPrecedingSymbolCount;
                            numSuffixes = std::distance(cache, curCache);
                        },
                        inputBegin_,
                        begin,
                        endForThisThread,
                        cache[threadId].data(),
                        std::ref(numSuffixes[threadId]),
                        sCount[threadId].data()));
                begin = endForThisThread;
            }
            for(auto & future: futures) {
                future.wait();
            }

            futures.clear();
            for(auto threadId = 0, begin = 0, numSymbolsPerThread = ((0x100 + numThreads - 1) / numThreads); threadId < numThreads; ++threadId) {
                auto end = begin + numSymbolsPerThread;
                if(end > 0x100)
                    end = 0x100;
                futures.push_back(
                    std::async(
                        [&dest, this, &sCount, numThreads](
                            int32_t begin,
                            int32_t end) {
                            for(auto threadId = 0; threadId < numThreads; ++threadId)
                                for(auto symbol = begin; symbol < end; ++symbol) {
                                    dest[threadId][symbol] = frontBucketOffset_[symbol];
                                    frontBucketOffset_[symbol] += sCount[threadId][symbol];
                                    sCount[threadId][symbol] = 0;
                                }
                        },
                        begin,
                        end));
                begin = end;
            }
            for(auto & future: futures) {
                future.wait();
            }

            futures.clear();
            for(auto threadId = 0; threadId < numThreads; ++threadId)
                futures.push_back(
                    std::async(
                        [](
                            int32_t * dest[0x100],
                            entry_type const * begin,
                            entry_type const * end) {
                            --begin;
                            while(++begin != end)
                                *(dest[begin->precedingSuffix_]++) = begin->precedingSuffixIndex_;
                        },
                        dest[threadId].data(),
                        cache[threadId].data(),
                        cache[threadId].data() + numSuffixes[threadId]));
            for(auto & future: futures) {
                future.wait();
            }
        }
    }

    void second_stage_its() {
        second_stage_its_right_to_left_pass_multi_threaded();
        second_stage_its_left_to_right_pass_multi_threaded();
    }

    void initial_two_byte_radix_sort(
        uint8_t const * begin,
        uint8_t const * end,
        int32_t * bStarOffset
    ) {
        if(begin < end)
            return;
        uint32_t state = 0;
        switch(get_suffix_type(begin)) {
            case suffix_type::a:
                state = 1;
                break;
            case suffix_type::b:
                state = 0;
                break;
            case suffix_type::bStar:
                state = 2;
                break;
        }
        auto current = begin;
        while(true) {
            if((state & 0x03) == 2) {
                int32_t flag = ((current > inputBegin_) && (current[-1] <= current[0])) ? 0 : PRECEDING_SUFFIX_IS_TYPE_A_FLAG;
                suffixArrayBegin_[bStarOffset[__builtin_bswap16(*(uint16_t const *)current)]++] =
                    (std::distance(inputBegin_, current) | flag);
            }
            if(--current < end)
                break;
            state <<= ((current[0] != current[1]) | ((state & 0x01) == 0));
            state |= (current[0] > current[1]);
        }
    }

    void first_stage_its(
        // private:
        // does the first stage of the improved two stage sort
    ) {
        auto numThreads = std::min((int32_t)std::thread::hardware_concurrency(), 2);
        std::unique_ptr<int32_t[]> bCount(new int32_t[0x10000]{});
        std::unique_ptr<int32_t[]> aCount(new int32_t[0x10000]{});
        std::unique_ptr<int32_t[]> bStarCount(new int32_t[numThreads * 0x10000]{});
        auto numSuffixesPerThread = ((std::distance(inputBegin_, inputEnd_) + numThreads - 1) / numThreads);

        {
            std::unique_ptr<int32_t[]> threadBCount(new int32_t[numThreads * 0x10000]{});
            std::unique_ptr<int32_t[]> threadACount(new int32_t[numThreads * 0x10000]{});
            auto inputCurrent = inputBegin_;
            futures.clear();
            for(auto threadId = 0; threadId < numThreads; ++threadId) {
                auto inputEnd = inputCurrent + numSuffixesPerThread;
                if(inputEnd > (inputEnd_ - 1))
                    inputEnd = (inputEnd_ - 1);
                auto arrayOffset = (threadId * 0x10000);
                std::array<int32_t *, 4> c({threadBCount.get() + arrayOffset, threadACount.get() + arrayOffset, bStarCount.get() + arrayOffset, threadACount.get() + arrayOffset});
                futures.push_back(
                    std::async(&msufsort::count_suffixes, this, inputEnd - 1, inputCurrent, c));
                inputCurrent = inputEnd;
            }
            for(auto & future: futures) {
                future.wait();
            }

            ++aCount[((uint16_t)inputEnd_[-1]) << 8];
            ++aCount_[inputEnd_[-1]];
            for(auto threadId = 0; threadId < numThreads; ++threadId) {
                auto arrayOffset = (threadId * 0x10000);
                for(auto j = 0; j < 0x10000; ++j) {
                    bCount[j] += threadBCount[arrayOffset + j];
                    bCount_[j >> 8] += threadBCount[arrayOffset + j] + bStarCount[arrayOffset + j];
                    aCount[j] += threadACount[arrayOffset + j];
                    aCount_[j >> 8] += threadACount[arrayOffset + j];
                }
            }
        }

        // compute bucket offsets into suffix array
        int32_t total = 1; // 1 for sentinel
        int32_t bStarTotal = 0;
        std::unique_ptr<int32_t[]> totalBStarCount(new int32_t[0x10000]{});
        std::unique_ptr<int32_t[]> bStarOffset(new int32_t[numThreads * 0x10000]{});
        std::unique_ptr<std::tuple<int32_t, int32_t, uint32_t>[]> partitions(new std::tuple<int32_t, int32_t, uint32_t>[0x10000] {
        });

        auto numPartitions = 0;
        for(int32_t i = 0; i < 0x100; ++i) {
            int32_t s = (i << 8);
            frontBucketOffset_[i] = (suffixArrayBegin_ + total);
            for(int32_t j = 0; j < 0x100; ++j, ++s) {
                auto partitionStartIndex = bStarTotal;
                for(int32_t threadId = 0; threadId < numThreads; ++threadId) {
                    bStarOffset[(threadId * 0x10000) + s] = bStarTotal;
                    totalBStarCount[s] += bStarCount[(threadId * 0x10000) + s];
                    bStarTotal += bStarCount[(threadId * 0x10000) + s];
                    bCount[s] += bStarCount[(threadId * 0x10000) + s];
                }
                total += (bCount[s] + aCount[s]);
                backBucketOffset_[(j << 8) | i] = suffixArrayBegin_ + total;
                if(totalBStarCount[s] > 0)
                    partitions[numPartitions++] = std::make_tuple(partitionStartIndex, totalBStarCount[s], (uint32_t)(s | j));
            }
        }

        auto inputCurrent = inputBegin_;
        futures.clear();
        for(auto threadId = 0; threadId < numThreads; ++threadId) {
            auto inputEnd = inputCurrent + numSuffixesPerThread;
            if(inputEnd > (inputEnd_ - 1))
                inputEnd = (inputEnd_ - 1);
            futures.push_back(
                std::async(&msufsort::initial_two_byte_radix_sort, this, inputEnd - 1, inputCurrent, &bStarOffset[threadId * 0x10000]));
            inputCurrent = inputEnd;
        }
        for(auto & future: futures) {
            future.wait();
        }

        // multikey quicksort on B* parititions
        std::atomic<int32_t> partitionCount(numPartitions);
        auto tandemRepeatStack = std::vector<std::vector<tandem_repeat_info>>(numThreads);
        // sort the partitions by size to ensure that the largest partitinos are not sorted last.
        // this prevents the case where the last thread is assigned a large thread while all other
        // threads exit due to no more partitions to sort.
        std::sort(std::execution::par_unseq, partitions.get(), partitions.get() + partitionCount.load(), [](std::tuple<int32_t, int32_t, uint32_t> const & a, std::tuple<int32_t, int32_t, uint32_t> const & b) -> bool {
            return (std::get<1>(a) < std::get<1>(b));
        });

        futures.clear();
        for(auto threadId = 0; threadId < numThreads; ++threadId) {
            tandemRepeatStack[threadId].reserve(1024);
            futures.push_back(
                std::async(
                    [&](
                        std::vector<tandem_repeat_info> & tandemRepeatStack) {
                        while(true) {
                            int32_t partitionIndex = --partitionCount;
                            if(partitionIndex < 0)
                                break;
                            auto const & partition = partitions[partitionIndex];
                            multikey_quicksort(suffixArrayBegin_ + std::get<0>(partition), suffixArrayBegin_ + std::get<0>(partition) + std::get<1>(partition), 2, 0, {0, std::get<2>(partition)}, tandemRepeatStack);
                        }
                    },
                    std::ref(tandemRepeatStack[threadId])));
        }
        for(auto & future: futures) {
            future.wait();
        }

        futures.clear();
        for(auto threadId = 0; threadId < numThreads; ++threadId) {
            futures.push_back(
                std::async(
                    [&](
                        std::vector<tandem_repeat_info> & tandemRepeatStack) {
                        complete_tandem_repeats(tandemRepeatStack);
                    },
                    std::ref(tandemRepeatStack[threadId])));
        }
        for(auto & future: futures) {
            future.wait();
        }

        // spread b* to their final locations in suffix array
        auto destination = suffixArrayBegin_ + total;
        auto source = suffixArrayBegin_ + bStarTotal;
        for(int32_t i = 0xffff; i >= 0; --i) {
            if(bCount[i] || aCount[i]) {
                destination -= bCount[i];
                source -= totalBStarCount[i];
                for(auto j = totalBStarCount[i] - 1; j >= 0; --j)
                    destination[j] = source[j];
                for(auto j = totalBStarCount[i]; j < bCount[i]; ++j)
                    destination[j] = SUFFIX_IS_UNSORTED_B_TYPE;
                destination -= aCount[i];
                for(auto j = 0; j < aCount[i]; ++j)
                    destination[j] = PRECEDING_SUFFIX_IS_TYPE_A_FLAG;
            }
        }
        suffixArrayBegin_[0] = (std::distance(inputBegin_, inputEnd_) | PRECEDING_SUFFIX_IS_TYPE_A_FLAG); // sa[0] = sentinel
    }

    uint8_t const * inputBegin_;
    uint8_t const * inputEnd_;
    int32_t * suffixArrayBegin_;
    int32_t * suffixArrayEnd_;
    int32_t * inverseSuffixArrayBegin_;
    int32_t * frontBucketOffset_[0x100];
    std::unique_ptr<int32_t *[]> backBucketOffset_;
    int32_t aCount_[0x100];
    int32_t bCount_[0x100];

    struct stack_frame {
        int32_t * suffixArrayBegin;
        int32_t * suffixArrayEnd;
        size_t currentMatchLength;
        uint32_t startingPattern;
        std::array<uint32_t, 2> endingPattern;
        std::vector<tandem_repeat_info> & tandemRepeatStack;
    };
    std::vector<std::future<void>> futures;

};


void calculate_suffix_array(
    rust::Slice<const uint8_t> input,
    rust::Slice<int32_t> output
) {
    msufsort msufsort_obj;
    return msufsort_obj.calculate_suffix_array(input, output);
}
