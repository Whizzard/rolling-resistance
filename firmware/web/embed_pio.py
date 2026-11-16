# PlatformIO-Haken: ruft web/embed.py vor jedem Build auf.
import subprocess, sys, os
Import("env")  # noqa: F821
here = os.path.join(env.subst("$PROJECT_DIR"), "web", "embed.py")  # noqa: F821
print(subprocess.run([sys.executable, here], capture_output=True, text=True).stdout.strip())
