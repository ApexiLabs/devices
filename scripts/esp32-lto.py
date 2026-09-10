"""Keep the classic ESP32 image inside its existing 2 MiB OTA slot."""
Import("env")

# pioarduino's prebuilt SDK explicitly disables LTO at link time. Match the
# application/core LTO objects with an enabled linker plugin and archive index.
env.ProcessUnFlags("-fno-lto")
env.Append(LINKFLAGS=["-flto"])
for variable, suffix in (("AR", "ar"), ("RANLIB", "ranlib")):
    command = env.subst("$" + variable)
    if not command.endswith("-gcc-" + suffix):
        env.Replace(**{variable: command.removesuffix("-" + suffix) + "-gcc-" + suffix})
