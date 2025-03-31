import os
import gzip
import subprocess
import filecmp
from shutil import copyfile

html_source = "CameraWeb.html"
html_gz = "CameraWeb.html.gz"
h_file_temp = "camera_index_temp.h"
h_file_target = "camera_index.h"  # Assuming header is in include/
array_name = "index_ov2640_html_gz"  # CHANGE THIS TO MATCH YOUR SENSOR/EXPECTED NAME

# Check if regeneration is needed (basic check, could use timestamps)
needs_regen = not os.path.exists(h_file_target) or \
              os.path.getmtime(html_source) > os.path.getmtime(h_file_target)

if needs_regen:
    print(f"Regenerating {h_file_target} from {html_source}...")
    try:
        # 1. Gzip
        with open(html_source, 'rb') as f_in, gzip.open(html_gz, 'wb') as f_out:
            f_out.writelines(f_in)

        # 2. xxd (or python equivalent) - Using xxd via subprocess here
        #    Make sure xxd is in the system PATH
        result = subprocess.run(['xxd', '-i', html_gz], capture_output=True, text=True, check=True)

        # 3. Format and write to temp header
        with open(h_file_temp, "w") as f_h:
            # Write guard if needed
            f_h.write("#pragma once\n\n")
            # Rename array and length variable in xxd output
            c_array_code = result.stdout.replace("unsigned char CameraWeb_html_gz[]", f"const uint8_t {array_name}[]")
            c_array_code = c_array_code.replace("unsigned int CameraWeb_html_gz_len",
                                                f"const unsigned int {array_name}_len")
            f_h.write(c_array_code)
            f_h.write("\n")  # Ensure newline at end

        # 4. Only copy if temp file is different from target to avoid unnecessary rebuilds
        if not os.path.exists(h_file_target) or not filecmp.cmp(h_file_temp, h_file_target, shallow=False):
            copyfile(h_file_temp, h_file_target)
            print(f"Updated {h_file_target}")
        else:
            print(f"{h_file_target} is already up-to-date.")

    except Exception as e:
        print(f"Error during HTML preparation: {e}")
        # Optionally delete temp files on error
        if os.path.exists(html_gz): os.remove(html_gz)
        if os.path.exists(h_file_temp): os.remove(h_file_temp)

    finally:
        # 5. Clean up temporary files
        if os.path.exists(html_gz): os.remove(html_gz)
        if os.path.exists(h_file_temp): os.remove(h_file_temp)
else:
    print(f"{h_file_target} is up-to-date. Skipping regeneration.")

# Add the header file directory to the include path

# Return("dummy") # Required for PlatformIO scripts
