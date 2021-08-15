fn main() {
    cxx_build::bridge("src/lib.rs")
        .file("src/msufsort.cc")
        .flag_if_supported("-std=c++11")
        .compile("msufsort");

    println!("cargo:rerun-if-changed=src/msufsort.cc");
    println!("cargo:rerun-if-changed=src/msufsort.h");
}
