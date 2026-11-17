# PlatformIO-Haken: backt die Oberflaeche der REGLERVARIANTE vor jedem Build.
import subprocess, sys, os
Import("env")  # noqa: F821
web = os.path.join(env.subst("$PROJECT_DIR"), "web")  # noqa: F821
r = subprocess.run([sys.executable, os.path.join(web, "embed.py"),
                    os.path.join(web, "vesc.html"),
                    os.path.join(env.subst("$PROJECT_DIR"), "src", "webui_vesc_html.h"),  # noqa: F821
                    "WEBUI_VESC_HTML_GZ"], capture_output=True, text=True)
print(r.stdout.strip() or r.stderr.strip())
