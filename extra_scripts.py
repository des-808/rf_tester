Import("env")

env.AddPostAction(
    "buildprog",
    env.VerboseAction("echo \"Build completed successfully\"", "")
)

# Add OpenOCD commands for proper reset handling
env.Replace(
    UPLOADFLAGS="-c \"adapter speed $UPLOAD_SPEED\" -c \"transport select swd\" -c \"set WORKAREASIZE 0x10000\" -c \"reset_config srst_nogate connect_assert_srst\" -c \"target reset\" -c \"shutdown\""
)
