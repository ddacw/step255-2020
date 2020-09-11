# Modified from
# https://github.com/bazelbuild/bazel/blob/master/third_party/zlib/BUILD

load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  #  BSD/MIT-like license

cc_library(
    name = "zlib",
    srcs = glob(["*.c"]),
    hdrs = glob(["*.h"]),
    # Use -Dverbose=-1 to turn off zlib's trace logging. (#3280)
    copts = [
        "-w",
        "-Dverbose=-1",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)
