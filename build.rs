fn main() {
    println!("cargo:rerun-if-changed=libsais.c");

    let src = [
        "src/libsais/libsais.c",
    ];
    let mut builder = cc::Build::new();
    let build = builder
        .files(src.iter())
        .flag("-Wno-unused-parameter");
    build.compile("libsais");
}
