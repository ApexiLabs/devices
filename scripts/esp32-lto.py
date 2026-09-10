"""Keep the classic ESP32 image inside its existing 2 MiB OTA slot."""
Import("env")

# pioarduino's prebuilt SDK explicitly disables LTO at link time. Match the
# application/core LTO objects with an enabled linker plugin and archive index.
env.ProcessUnFlags("-fno-lto")
env.Append(LINKFLAGS=["-flto"])
env.Replace(AR=env.subst("$AR").replace("-ar", "-gcc-ar"),
            RANLIB=env.subst("$RANLIB").replace("-ranlib", "-gcc-ranlib"))
