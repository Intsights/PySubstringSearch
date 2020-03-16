#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <array>
#include <vector>

namespace maniscalco {

    class msufsort {
        public:
        using suffix_array = std::vector<std::int32_t>;
        using suffix_value = std::uint32_t;

        msufsort(std::int32_t = 1);
        ~msufsort();

        suffix_array make_suffix_array(
            std::uint8_t const *,
            std::uint8_t const *);

        private:
        static std::int32_t constexpr is_induced_sort = 0x40000000;
        static std::int32_t constexpr is_tandem_repeat_length = 0x80000000;
        static std::int32_t constexpr isa_flag_mask = is_induced_sort | is_tandem_repeat_length;
        static std::int32_t constexpr isa_index_mask = ~isa_flag_mask;

        static std::int32_t constexpr preceding_suffix_is_type_a_flag = 0x80000000;
        static std::int32_t constexpr mark_isa_when_sorted = 0x40000000;
        static std::int32_t constexpr sa_index_mask = ~(preceding_suffix_is_type_a_flag | mark_isa_when_sorted);
        static std::int32_t constexpr suffix_is_unsorted_b_type = sa_index_mask;

        static constexpr std::int32_t insertion_sort_threshold = 16;
        static std::int32_t constexpr min_match_length_for_tandem_repeats = (2 + sizeof(suffix_value) + sizeof(suffix_value));

        enum suffix_type {
            a,
            b,
            bStar
        };

        struct tandem_repeat_info {
            tandem_repeat_info(
                std::int32_t * partitionBegin,
                std::int32_t * partitionEnd,
                std::int32_t numTerminators,
                std::int32_t tandemRepeatLength):
                partitionBegin_(partitionBegin),
                partitionEnd_(partitionEnd),
                numTerminators_(numTerminators),
                tandemRepeatLength_(tandemRepeatLength) {
            }

            std::int32_t * partitionBegin_;
            std::int32_t * partitionEnd_;
            std::int32_t numTerminators_;
            std::int32_t tandemRepeatLength_;
        };

        suffix_value get_value(
            uint8_t const *,
            std::int32_t) const;

        suffix_type get_suffix_type(
            uint8_t const *);

        bool compare_suffixes(
            std::uint8_t const *,
            std::int32_t,
            std::int32_t) const;

        void insertion_sort(
            std::int32_t *,
            std::int32_t *,
            int32_t,
            uint64_t);

        void multikey_insertion_sort(
            std::int32_t *,
            std::int32_t *,
            std::int32_t,
            suffix_value,
            std::array<suffix_value, 2>,
            std::vector<tandem_repeat_info> &);

        std::size_t partition_tandem_repeats(
            std::int32_t *,
            std::int32_t *,
            std::int32_t,
            std::vector<tandem_repeat_info> &);

        void count_suffixes(
            uint8_t const *,
            uint8_t const *,
            std::array<int32_t *, 4>);

        void second_stage_its();

        void second_stage_its_right_to_left_pass_single_threaded();

        void second_stage_its_right_to_left_pass_multi_threaded();

        void second_stage_its_left_to_right_pass_single_threaded();

        void second_stage_its_left_to_right_pass_multi_threaded();

        void first_stage_its();

        std::int32_t * multikey_quicksort(
            std::int32_t *,
            std::int32_t *,
            std::size_t,
            suffix_value,
            std::array<suffix_value, 2>,
            std::vector<tandem_repeat_info> &);

        void initial_two_byte_radix_sort(
            uint8_t const *,
            uint8_t const *,
            int32_t *);

        bool has_potential_tandem_repeats(
            suffix_value,
            std::array<suffix_value, 2>) const;

        void complete_tandem_repeats(
            std::vector<tandem_repeat_info> &);

        void complete_tandem_repeat(
            std::int32_t *,
            std::int32_t *,
            std::int32_t,
            std::int32_t);

        uint8_t const * inputBegin_;

        uint8_t const * inputEnd_;

        int32_t inputSize_;

        uint8_t const * getValueEnd_;

        std::int32_t getValueMaxIndex_;

        uint8_t copyEnd_[sizeof(suffix_value) << 1];

        std::int32_t * suffixArrayBegin_;

        std::int32_t * suffixArrayEnd_;

        std::int32_t * inverseSuffixArrayBegin_;

        std::int32_t * inverseSuffixArrayEnd_;

        std::int32_t * frontBucketOffset_[0x100];

        std::unique_ptr<std::int32_t *[]> backBucketOffset_;

        int32_t aCount_[0x100];

        int32_t bCount_[0x100];

        bool const tandemRepeatSortEnabled_ = true;

        int32_t numWorkerThreads_;
        struct stack_frame {
            std::int32_t * suffixArrayBegin;
            std::int32_t * suffixArrayEnd;
            std::size_t currentMatchLength;
            suffix_value startingPattern;
            std::array<suffix_value, 2> endingPattern;
            std::vector<tandem_repeat_info> & tandemRepeatStack;
        };
        std::vector<std::future<void>> futures;

    }; // class msufsort

    template<typename input_iter>
    msufsort::suffix_array make_suffix_array(
        input_iter,
        input_iter,
        int32_t = 1);

} // namespace maniscalco

template<typename input_iter>
maniscalco::msufsort::suffix_array maniscalco::make_suffix_array(
    input_iter begin,
    input_iter end,
    int32_t numThreads) {
    if(numThreads <= 0)
        numThreads = 1;
    if(numThreads > (int32_t)std::thread::hardware_concurrency())
        numThreads = (int32_t)std::thread::hardware_concurrency();
    return msufsort(numThreads).make_suffix_array((uint8_t const *)&*begin, (uint8_t const *)&*end);
}

maniscalco::msufsort::msufsort(
    int32_t numThreads):
    inputBegin_(nullptr),
    inputEnd_(nullptr),
    inputSize_(),
    getValueEnd_(nullptr),
    getValueMaxIndex_(),
    copyEnd_(),
    suffixArrayBegin_(nullptr),
    suffixArrayEnd_(nullptr),
    inverseSuffixArrayBegin_(nullptr),
    inverseSuffixArrayEnd_(nullptr),
    frontBucketOffset_(),
    backBucketOffset_(new std::int32_t * [0x10000] {
    }),
    aCount_(),
    bCount_(),
    numWorkerThreads_(numThreads - 1) {
}

maniscalco::msufsort::~msufsort() {
}

inline auto maniscalco::msufsort::get_suffix_type(
    uint8_t const * suffix) -> suffix_type {
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

inline auto maniscalco::msufsort::get_value(
    uint8_t const * inputCurrent,
    std::int32_t index) const -> suffix_value {
    inputCurrent += (index & sa_index_mask);
    if(inputCurrent >= getValueEnd_) {
        if(inputCurrent >= inputEnd_)
            return 0;
        inputCurrent = (copyEnd_ + (sizeof(suffix_value) - std::distance(inputCurrent, inputEnd_)));
    }
    return __builtin_bswap32(*(suffix_value const *)(inputCurrent));
}

inline bool maniscalco::msufsort::compare_suffixes(
    // optimized compare_suffixes for when two suffixes have long common match lengths
    std::uint8_t const * inputBegin,
    std::int32_t indexA,
    std::int32_t indexB) const {
    indexA &= sa_index_mask;
    indexB &= sa_index_mask;

    if(indexA > indexB)
        return !compare_suffixes(inputBegin, indexB, indexA);

    auto inputCurrentA = inputBegin + indexA;
    auto inputCurrentB = inputBegin + indexB;
    while((inputCurrentB <= getValueEnd_) && (*(suffix_value const *)inputCurrentB == *(suffix_value const *)inputCurrentA)) {
        inputCurrentB += sizeof(suffix_value);
        inputCurrentA += sizeof(suffix_value);
    }
    if(inputCurrentB >= getValueEnd_) {
        if(inputCurrentB >= inputEnd_)
            return true;
        inputCurrentB = (copyEnd_ + (sizeof(suffix_value) - std::distance(inputCurrentB, inputEnd_)));
    }
    auto valueB = __builtin_bswap32(*(suffix_value const *)(inputCurrentB));

    if(inputCurrentA >= getValueEnd_)
        inputCurrentA = (copyEnd_ + (sizeof(suffix_value) - std::distance(inputCurrentA, inputEnd_)));
    auto valueA = __builtin_bswap32(*(suffix_value const *)(inputCurrentA));
    return (valueA >= valueB);
}

void maniscalco::msufsort::multikey_insertion_sort(
    // private:
    // sorts the suffixes by insertion sort
    std::int32_t * partitionBegin,
    std::int32_t * partitionEnd,
    std::int32_t currentMatchLength,
    suffix_value startingPattern,
    std::array<suffix_value, 2> endingPattern,
    std::vector<tandem_repeat_info> & tandemRepeatStack) {
    std::int32_t partitionSize = (std::int32_t)std::distance(partitionBegin, partitionEnd);
    if(partitionSize < 2)
        return;
    struct partition_info {
        std::int32_t currentMatchLength_;
        std::int32_t size_;
        suffix_value startingPattern_;
        suffix_value endingPattern_;
        bool hasPotentialTandemRepeats_;
    };
    partition_info stack[insertion_sort_threshold];
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
            if(currentMatchLength >= min_match_length_for_tandem_repeats) {
                if(hasPotentialTandemRepeats) {
                    auto tandemRepeatCount = partition_tandem_repeats(partitionBegin, partitionBegin + size, currentMatchLength, tandemRepeatStack);
                    size -= tandemRepeatCount;
                    partitionBegin += tandemRepeatCount;
                    if(size == 0)
                        continue;
                }
            }
            suffix_value value[insertion_sort_threshold];
            value[0] = get_value(inputBegin_ + currentMatchLength, partitionBegin[0]);
            for(std::int32_t i = 1; i < size; ++i) {
                auto currentIndex = partitionBegin[i];
                suffix_value currentValue = get_value(inputBegin_ + currentMatchLength, partitionBegin[i]);
                auto j = i;
                while((j > 0) && (value[j - 1] > currentValue)) {
                    value[j] = value[j - 1];
                    partitionBegin[j] = partitionBegin[j - 1];
                    --j;
                }
                value[j] = currentValue;
                partitionBegin[j] = currentIndex;
            }

            auto i = (std::int32_t)size - 1;
            auto nextMatchLength = currentMatchLength + (std::int32_t)sizeof(suffix_value);
            while(i >= 0) {
                std::int32_t start = i--;
                auto startValue = value[start];
                while((i >= 0) && (value[i] == startValue))
                    --i;
                auto partitionSize = (start - i);
                auto potentialTandemRepeats = has_potential_tandem_repeats(startingPattern, {endingPattern[0], startValue});
                if(nextMatchLength == (2 + sizeof(suffix_value)))
                    startingPattern = get_value(inputBegin_, *partitionBegin);
                *stackTop++ = partition_info{nextMatchLength, partitionSize, startingPattern, startValue, potentialTandemRepeats};
            }
        }
    }
}

inline bool maniscalco::msufsort::has_potential_tandem_repeats(
    suffix_value startingPattern,
    std::array<suffix_value, 2> endingPattern) const {
    if(!tandemRepeatSortEnabled_)
        return false;
    std::int8_t const * end = (std::int8_t const *)endingPattern.data();
    std::int8_t const * begin = end + sizeof(suffix_value);
    while(begin > end)
        if(*(suffix_value const *)--begin == *(suffix_value *)&startingPattern)
            return true;
    return false;
}

std::size_t maniscalco::msufsort::partition_tandem_repeats(
    // private:
    // the tandem repeat sort.  determines if the suffixes provided are tandem repeats
    // of other suffixes from within the same group.  If so, sorts the non tandem
    // repeat suffixes and then induces the sorted order of the suffixes which are
    // tandem repeats.
    std::int32_t * partitionBegin,
    std::int32_t * partitionEnd,
    std::int32_t currentMatchLength,
    std::vector<tandem_repeat_info> & tandemRepeatStack) {
    auto parititionSize = std::distance(partitionBegin, partitionEnd);
    std::sort(partitionBegin, partitionEnd, [](std::int32_t a, std::int32_t b) -> bool {
        return ((a & sa_index_mask) < (b & sa_index_mask));
    });
    std::int32_t tandemRepeatLength = 0;
    auto const halfCurrentMatchLength = (currentMatchLength >> 1);

    // determine if there are tandem repeats and, if so, what the tandem repeat length is.
    auto previousSuffixIndex = (partitionBegin[0] & sa_index_mask);
    for(auto cur = partitionBegin + 1; ((tandemRepeatLength == 0) && (cur < partitionEnd)); ++cur) {
        auto currentSuffixIndex = (*cur & sa_index_mask);
        if((previousSuffixIndex + halfCurrentMatchLength) >= currentSuffixIndex)
            tandemRepeatLength = (currentSuffixIndex - previousSuffixIndex);
        previousSuffixIndex = currentSuffixIndex;
    }
    if(tandemRepeatLength == 0)
        return 0; // no tandem repeats were found
    // tandem repeats detected.
    std::int32_t * terminatorsEnd = partitionEnd - 1;
    previousSuffixIndex = (partitionEnd[-1] & sa_index_mask);
    for(auto cur = partitionEnd - 2; cur >= partitionBegin; --cur) {
        auto currentSuffixIndex = (*cur & sa_index_mask);
        if((previousSuffixIndex - currentSuffixIndex) == tandemRepeatLength)
            std::swap(*terminatorsEnd--, *cur); // suffix is a tandem repeat
        previousSuffixIndex = currentSuffixIndex;
    }
    auto numTerminators = (std::distance(partitionBegin, terminatorsEnd) + 1);
    std::reverse(partitionBegin, partitionEnd);
    tandemRepeatStack.push_back(tandem_repeat_info(partitionBegin, partitionEnd, (std::int32_t)numTerminators, tandemRepeatLength));
    return (parititionSize - numTerminators);
}

void maniscalco::msufsort::complete_tandem_repeats(
    std::vector<tandem_repeat_info> & tandemRepeatStack) {
    while(!tandemRepeatStack.empty()) {
        tandem_repeat_info tandemRepeat = tandemRepeatStack.back();
        tandemRepeatStack.pop_back();
        complete_tandem_repeat(tandemRepeat.partitionBegin_, tandemRepeat.partitionEnd_, tandemRepeat.numTerminators_, tandemRepeat.tandemRepeatLength_);
    }
}

inline void maniscalco::msufsort::complete_tandem_repeat(
    std::int32_t * partitionBegin,
    std::int32_t * partitionEnd,
    std::int32_t numTerminators,
    std::int32_t tandemRepeatLength) {
    std::int32_t * terminatorsBegin = partitionEnd - numTerminators;
    for(auto cur = terminatorsBegin - 1; cur >= partitionBegin; --cur) {
        auto currentSuffixIndex = (*cur & sa_index_mask);
        inverseSuffixArrayBegin_[currentSuffixIndex >> 1] = (tandemRepeatLength | is_tandem_repeat_length);
    }
    // now use sorted order of terminators to determine sorted order of repeats.
    // figure out how many terminators sort before the repeat and how
    // many sort after the repeat.  put them on left and right extremes of the array.
    std::int32_t m = 0;
    std::int32_t a = 0;
    std::int32_t b = numTerminators - 1;
    std::int32_t numTypeA = 0;
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
    std::int32_t numTypeB = (numTerminators - numTypeA);

    for(std::int32_t i = 0; i < numTypeA; ++i)
        partitionBegin[i] = terminatorsBegin[i];

    // type A repeats
    auto current = partitionBegin;
    auto currentEnd = current + numTypeA;
    auto next = currentEnd;
    while(current != currentEnd) {
        while(current != currentEnd) {
            auto index = (*current++ & sa_index_mask);
            if(index >= tandemRepeatLength) {
                auto potentialTandemRepeatIndex = index - tandemRepeatLength;
                auto isaValue = inverseSuffixArrayBegin_[potentialTandemRepeatIndex >> 1];
                if((isaValue & is_tandem_repeat_length) && ((isaValue & isa_index_mask) == tandemRepeatLength)) {
                    auto flag = ((potentialTandemRepeatIndex > 0) && (inputBegin_[potentialTandemRepeatIndex - 1] <= inputBegin_[potentialTandemRepeatIndex])) ? 0 : preceding_suffix_is_type_a_flag;
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
            auto index = (*current-- & sa_index_mask);
            if(index >= tandemRepeatLength) {
                auto potentialTandemRepeatIndex = index - tandemRepeatLength;
                auto isaValue = inverseSuffixArrayBegin_[potentialTandemRepeatIndex >> 1];
                if((isaValue & is_tandem_repeat_length) && ((isaValue & isa_index_mask) == tandemRepeatLength)) {
                    auto flag = ((potentialTandemRepeatIndex > 0) && (inputBegin_[potentialTandemRepeatIndex - 1] <= inputBegin_[potentialTandemRepeatIndex])) ? 0 : preceding_suffix_is_type_a_flag;
                    *(next) = (potentialTandemRepeatIndex | flag);
                    --next;
                }
            }
        }
        currentEnd = next;
    }
}

auto maniscalco::msufsort::multikey_quicksort(
    // private:
    // multi key quicksort on the input data provided
    std::int32_t * suffixArrayBegin,
    std::int32_t * suffixArrayEnd,
    std::size_t currentMatchLength,
    suffix_value startingPattern,
    std::array<suffix_value, 2> endingPattern,
    std::vector<tandem_repeat_info> & tandemRepeatStack) -> std::int32_t * {
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

        std::uint64_t partitionSize = std::distance(suffixArrayBegin, suffixArrayEnd);
        if (partitionSize < 2)
            continue;

        if (currentMatchLength >= min_match_length_for_tandem_repeats)
        {
            if (currentMatchLength == min_match_length_for_tandem_repeats)
                startingPattern = get_value(inputBegin_, *suffixArrayBegin);
            if ((partitionSize > 1) && (has_potential_tandem_repeats(startingPattern, endingPattern)))
                suffixArrayBegin += partition_tandem_repeats(suffixArrayBegin, suffixArrayEnd, currentMatchLength, tandemRepeatStack);
            partitionSize = std::distance(suffixArrayBegin, suffixArrayEnd);
        }

        if (partitionSize < insertion_sort_threshold)
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
            stack.push_back({beginPivot3, endPivot3, (currentMatchLength + sizeof(suffix_value)), startingPattern, {endingPattern[1], pivot3}, tandemRepeatStack});
        if (++endPivot2 != beginPivot3)
            stack.push_back({endPivot2, beginPivot3, currentMatchLength, startingPattern, endingPattern, tandemRepeatStack});
        if (beginPivot2 != endPivot2)
            stack.push_back({beginPivot2, endPivot2, (currentMatchLength + sizeof(suffix_value)), startingPattern, {endingPattern[1], pivot2}, tandemRepeatStack});
        if (endPivot1 != beginPivot2)
            stack.push_back({endPivot1, beginPivot2, currentMatchLength, startingPattern, endingPattern, tandemRepeatStack});
        if (beginPivot1 != endPivot1)
            stack.push_back({beginPivot1, endPivot1, (currentMatchLength + sizeof(suffix_value)), startingPattern, {endingPattern[1], pivot1}, tandemRepeatStack});
        if (suffixArrayBegin != beginPivot1)
            stack.push_back({suffixArrayBegin, beginPivot1, currentMatchLength, startingPattern, endingPattern, tandemRepeatStack});
    }
    return suffixArrayEnd;
}

void maniscalco::msufsort::second_stage_its_right_to_left_pass_multi_threaded(
    // private:
    // induce sorted position of B suffixes from sorted B* suffixes
    // This is the first half of the second stage of the ITS ... the 'right to left' pass
) {
    auto numThreads = (int32_t)(numWorkerThreads_ + 1); // +1 for main thread
    auto max_cache_size = (1 << 12);
    struct entry_type {
        uint8_t precedingSuffix_;
        int32_t precedingSuffixIndex_;
    };
    std::unique_ptr<entry_type[]> cache[numThreads];
    for(auto i = 0; i < numThreads; ++i)
        cache[i].reset(new entry_type[max_cache_size]);
    int32_t numSuffixes[numThreads] = {};
    int32_t sCount[numThreads][0x100] = {};
    std::int32_t * dest[numThreads][0x100] = {};

    auto currentSuffix = suffixArrayBegin_ + inputSize_;
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
            while((temp > maxEnd) && (*temp != suffix_is_unsorted_b_type))
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
                            std::int32_t * begin,
                            std::int32_t * end,
                            entry_type * cache,
                            int32_t & numSuffixes,
                            int32_t * suffixCount) {
                            auto curCache = cache;
                            ++begin;
                            uint8_t currentPrecedingSymbol = 0;
                            int32_t currentPrecedingSymbolCount = 0;
                            while(--begin > end) {
                                if((*begin & preceding_suffix_is_type_a_flag) == 0) {
                                    int32_t precedingSuffixIndex = ((*begin & sa_index_mask) - 1);
                                    auto precedingSuffix = (inputBegin + precedingSuffixIndex);
                                    auto precedingSymbol = precedingSuffix[0];
                                    int32_t flag = ((precedingSuffixIndex > 0) && (precedingSuffix[-1] <= precedingSymbol)) ? 0 : preceding_suffix_is_type_a_flag;
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
                        cache[threadId].get(),
                        std::ref(numSuffixes[threadId]),
                        sCount[threadId]));
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
                            std::int32_t * dest[0x100],
                            entry_type const * begin,
                            entry_type const * end) {
                            --begin;
                            while(++begin < end)
                                *(--dest[begin->precedingSuffix_]) = begin->precedingSuffixIndex_;
                        },
                        dest[threadId],
                        cache[threadId].get(),
                        cache[threadId].get() + numSuffixes[threadId]));
            for(auto & future: futures) {
                future.wait();
            }
        }
        currentSuffix -= aCount_[symbol];
    }
}

void maniscalco::msufsort::second_stage_its_right_to_left_pass_single_threaded(
    // private:
    // induce sorted position of B suffixes from sorted B* suffixes
    // This is the first half of the second stage of the ITS ... the 'right to left' pass
) {
    auto currentSuffix = suffixArrayBegin_ + inputSize_;
    for(auto i = 0xff; i >= 0; --i) {
        auto backBucketOffset = &backBucketOffset_[i << 8];
        auto prevWrite = backBucketOffset;
        int32_t previousPrecedingSymbol = 0;
        auto endSuffix = currentSuffix - bCount_[i];
        while(currentSuffix > endSuffix) {
            if((*currentSuffix & preceding_suffix_is_type_a_flag) == 0) {
                int32_t precedingSuffixIndex = ((*currentSuffix & sa_index_mask) - 1);
                auto precedingSuffix = (inputBegin_ + precedingSuffixIndex);
                auto precedingSymbol = precedingSuffix[0];
                int32_t flag = ((precedingSuffixIndex > 0) && (precedingSuffix[-1] <= precedingSymbol)) ? 0 : preceding_suffix_is_type_a_flag;
                if(precedingSymbol != previousPrecedingSymbol) {
                    previousPrecedingSymbol = precedingSymbol;
                    prevWrite = backBucketOffset + previousPrecedingSymbol;
                }
                *(--*prevWrite) = (precedingSuffixIndex | flag);
            }
            --currentSuffix;
        }
        currentSuffix -= aCount_[i];
    }
}

void maniscalco::msufsort::second_stage_its_left_to_right_pass_single_threaded(
    // private:
    // induce sorted position of A suffixes from sorted B suffixes
    // This is the second half of the second stage of the ITS ... the 'left to right' pass
) {
    auto currentSuffix = suffixArrayBegin_ - 1;
    uint8_t previousPrecedingSymbol = 0;
    auto previousFrontBucketOffset = frontBucketOffset_;
    while(++currentSuffix < suffixArrayEnd_) {
        auto currentSuffixIndex = *currentSuffix;
        if(currentSuffixIndex & preceding_suffix_is_type_a_flag) {
            if((currentSuffixIndex & sa_index_mask) != 0) {
                int32_t precedingSuffixIndex = ((currentSuffixIndex & sa_index_mask) - 1);
                auto precedingSuffix = (inputBegin_ + precedingSuffixIndex);
                auto precedingSymbol = precedingSuffix[0];
                int32_t flag = ((precedingSuffixIndex > 0) && (precedingSuffix[-1] >= precedingSymbol)) ? preceding_suffix_is_type_a_flag : 0;
                if(precedingSymbol != previousPrecedingSymbol) {
                    previousPrecedingSymbol = precedingSymbol;
                    previousFrontBucketOffset = frontBucketOffset_ + previousPrecedingSymbol;
                }
                *((*previousFrontBucketOffset)++) = (precedingSuffixIndex | flag);
            }
            *(currentSuffix) &= sa_index_mask;
        }
    }
}

void maniscalco::msufsort::second_stage_its_left_to_right_pass_multi_threaded(
    // private:
    // induce sorted position of A suffixes from sorted B suffixes
    // This is the second half of the second stage of the ITS ... the 'left to right' pass
) {
    auto numThreads = (int32_t)(numWorkerThreads_ + 1); // +1 for main thread
    auto currentSuffix = suffixArrayBegin_;
    auto max_cache_size = (1 << 12);
    struct entry_type {
        uint8_t precedingSuffix_;
        int32_t precedingSuffixIndex_;
    };
    std::unique_ptr<entry_type[]> cache[numThreads];
    for(auto i = 0; i < numThreads; ++i)
        cache[i].reset(new entry_type[max_cache_size]);
    int32_t numSuffixes[numThreads] = {};
    int32_t sCount[numThreads][0x100] = {};
    std::int32_t * dest[numThreads][0x100] = {};

    while(currentSuffix < suffixArrayEnd_) {
        // calculate current 'safe' suffixes to process
        while((currentSuffix < suffixArrayEnd_) && (!(*currentSuffix & preceding_suffix_is_type_a_flag)))
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
                        std::int32_t * begin,
                        std::int32_t * end,
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
                            if(currentSuffixIndex & preceding_suffix_is_type_a_flag) {
                                currentSuffixIndex &= sa_index_mask;
                                if(currentSuffixIndex != 0) {
                                    int32_t precedingSuffixIndex = (currentSuffixIndex - 1);
                                    auto precedingSuffix = (inputBegin + precedingSuffixIndex);
                                    auto precedingSymbol = precedingSuffix[0];
                                    int32_t flag = ((precedingSuffixIndex > 0) && (precedingSuffix[-1] >= precedingSymbol)) ? preceding_suffix_is_type_a_flag : 0;
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
                    cache[threadId].get(),
                    std::ref(numSuffixes[threadId]),
                    sCount[threadId]));
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
                        std::int32_t * dest[0x100],
                        entry_type const * begin,
                        entry_type const * end) {
                        --begin;
                        while(++begin != end)
                            *(dest[begin->precedingSuffix_]++) = begin->precedingSuffixIndex_;
                    },
                    dest[threadId],
                    cache[threadId].get(),
                    cache[threadId].get() + numSuffixes[threadId]));
        for(auto & future: futures) {
            future.wait();
        }
    }
}

void maniscalco::msufsort::second_stage_its(
    // private:
    // performs the the second stage of the improved two stage sort.
) {
    if(numWorkerThreads_ == 0) {
        second_stage_its_right_to_left_pass_single_threaded();
        second_stage_its_left_to_right_pass_single_threaded();
    } else {
        second_stage_its_right_to_left_pass_multi_threaded();
        second_stage_its_left_to_right_pass_multi_threaded();
    }
}

void maniscalco::msufsort::count_suffixes(
    uint8_t const * begin,
    uint8_t const * end,
    std::array<int32_t *, 4> count) {
    if(begin < end)
        return;
    std::uint32_t state = 0;
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

void maniscalco::msufsort::initial_two_byte_radix_sort(
    uint8_t const * begin,
    uint8_t const * end,
    int32_t * bStarOffset) {
    if(begin < end)
        return;
    std::uint32_t state = 0;
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
            int32_t flag = ((current > inputBegin_) && (current[-1] <= current[0])) ? 0 : preceding_suffix_is_type_a_flag;
            suffixArrayBegin_[bStarOffset[__builtin_bswap16(*(uint16_t const *)current)]++] =
                (std::distance(inputBegin_, current) | flag);
        }
        if(--current < end)
            break;
        state <<= ((current[0] != current[1]) | ((state & 0x01) == 0));
        state |= (current[0] > current[1]);
    }
}

void maniscalco::msufsort::first_stage_its(
    // private:
    // does the first stage of the improved two stage sort
) {
    auto numThreads = (int32_t)(numWorkerThreads_ + 1);
    std::unique_ptr<int32_t[]> bCount(new int32_t[0x10000]{});
    std::unique_ptr<int32_t[]> aCount(new int32_t[0x10000]{});
    std::unique_ptr<int32_t[]> bStarCount(new int32_t[numThreads * 0x10000]{});
    auto numSuffixesPerThread = ((inputSize_ + numThreads - 1) / numThreads);

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
    std::unique_ptr<std::tuple<std::int32_t, std::int32_t, suffix_value>[]> partitions(new std::tuple<std::int32_t, std::int32_t, suffix_value>[0x10000] {
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
                partitions[numPartitions++] = std::make_tuple(partitionStartIndex, totalBStarCount[s], (suffix_value)(s | j));
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
    std::atomic<std::int32_t> partitionCount(numPartitions);
    std::vector<tandem_repeat_info> tandemRepeatStack[numThreads];
    // sort the partitions by size to ensure that the largest partitinos are not sorted last.
    // this prevents the case where the last thread is assigned a large thread while all other
    // threads exit due to no more partitions to sort.
    std::sort(partitions.get(), partitions.get() + partitionCount.load(), [](std::tuple<std::int32_t, std::int32_t, suffix_value> const & a, std::tuple<std::int32_t, std::int32_t, suffix_value> const & b) -> bool {
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
                        std::int32_t partitionIndex = --partitionCount;
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
                destination[j] = suffix_is_unsorted_b_type;
            destination -= aCount[i];
            for(auto j = 0; j < aCount[i]; ++j)
                destination[j] = preceding_suffix_is_type_a_flag;
        }
    }
    suffixArrayBegin_[0] = (inputSize_ | preceding_suffix_is_type_a_flag); // sa[0] = sentinel
}

auto maniscalco::msufsort::make_suffix_array(
    uint8_t const * inputBegin,
    uint8_t const * inputEnd) -> suffix_array {
    inputBegin_ = inputBegin;
    inputEnd_ = inputEnd;
    inputSize_ = std::distance(inputBegin_, inputEnd_);
    getValueEnd_ = (inputEnd_ - sizeof(suffix_value));
    getValueMaxIndex_ = (inputSize_ - sizeof(suffix_value));
    for(auto & e: copyEnd_)
        e = 0x00;
    auto source = inputEnd_ - sizeof(suffix_value);
    auto dest = copyEnd_;
    if(source < inputBegin_) {
        auto n = std::distance(source, inputBegin);
        source += n;
        dest += n;
    }
    std::copy(source, inputEnd_, dest);
    suffix_array suffixArray;
    auto suffixArraySize = (inputSize_ + 1);
    suffixArray.resize(suffixArraySize);
    for(auto & e: suffixArray)
        e = 0;
    suffixArrayBegin_ = suffixArray.data();
    suffixArrayEnd_ = suffixArrayBegin_ + suffixArraySize;
    inverseSuffixArrayBegin_ = (suffixArrayBegin_ + ((inputSize_ + 1) >> 1));
    inverseSuffixArrayEnd_ = suffixArrayEnd_;

    first_stage_its();
    second_stage_its();
    return suffixArray;
}
