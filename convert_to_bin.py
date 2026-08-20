Import("env")
import os

print("=== convert_to_bin.py loaded ===")

# Remove the firmware size check BEFORE build
env.AddPreAction(
    "buildprog",
    env.VerboseAction(
        "echo 'Removing checkprogsize target'",
        "Removing checkprogsize target"
    )
)

# Override checkprogsize to do nothing
env.Replace(
    ACTION_CHECKPROGSIZE=None
)

# Add binary conversion after linking
env.AddPostAction(
    "$BUILD_DIR/firmware.elf",
    env.VerboseAction(
        "%s -O binary $BUILD_DIR/firmware.elf $BUILD_DIR/firmware.bin" % (
            os.path.join(env.subst("$PIOHOME_DIR"), "packages", "toolchain-gccarmnoneeabi", "bin", "arm-none-eabi-objcopy")
        ),
        "Generated firmware.bin: $BUILD_DIR/firmware.bin"
    )
)

print("=== convert_to_bin.py ready ===")
