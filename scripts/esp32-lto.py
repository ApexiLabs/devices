"""Keep the classic ESP32 image inside its existing 2 MiB OTA slot."""
Import("env")

# pioarduino's prebuilt SDK explicitly disables LTO at link time. Match the
# application/core LTO objects with an enabled linker plugin and archive index.
env.ProcessUnFlags("-fno-lto")
# One LTO partition lets GCC optimize across all application units and keeps
# recovery code within the existing 2 MiB OTA slots, without repartitioning.
env.Append(LINKFLAGS=["-flto", "-flto-partition=one"])
for variable, suffix in (("AR", "ar"), ("RANLIB", "ranlib")):
    command = env.subst("$" + variable)
    if not command.endswith("-gcc-" + suffix):
        env.Replace(**{variable: command.removesuffix("-" + suffix) + "-gcc-" + suffix})
