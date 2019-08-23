diff --git "a/C:\\Users\\USER\\AppData\\Local\\Temp\\TortoiseGit\\con6AAB.tmp\\conf_common-913a06a-left.py" "b/C:\\Users\\USER\\ESP8266\\ESP8266_RTOS_SDK_v3.1\\docs\\conf_common.py"
index c011ef94..f0e9cd6b 100644
--- "a/C:\\Users\\USER\\AppData\\Local\\Temp\\TortoiseGit\\con6AAB.tmp\\conf_common-913a06a-left.py"
+++ "b/C:\\Users\\USER\\ESP8266\\ESP8266_RTOS_SDK_v3.1\\docs\\conf_common.py"
@@ -64,10 +64,10 @@ kconfig_inc_path = '{}/inc/kconfig.inc'.format(builddir)
 temp_sdkconfig_path = '{}/sdkconfig.tmp'.format(builddir)
 # note: trimming "examples" dir from KConfig/KConfig.projbuild as MQTT submodule
 # has its own examples in the submodule.
-kconfigs = subprocess.check_output(["find", "../../components",
+kconfigs = subprocess.check_output(["find_msys", "../../components",
                                     "-name", "examples", "-prune",
                                     "-o", "-name", "Kconfig", "-print"]).decode()
-kconfig_projbuilds = subprocess.check_output(["find", "../../components",
+kconfig_projbuilds = subprocess.check_output(["find_msys", "../../components",
                                               "-name", "examples", "-prune",
                                               "-o", "-name", "Kconfig.projbuild", "-print"]).decode()
 confgen_args = [sys.executable,
