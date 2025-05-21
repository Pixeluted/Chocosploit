import re
import sys
from pathlib import Path

source_dir = Path(sys.argv[1])
libs_dir = source_dir / "Environment/Libraries"

all_libraries = [file for file in libs_dir.rglob("*.cpp")]

custom_func_pattern = r"int\s+(\w+)\s*\(\s*lua_State\s*\*\s*L\s*\)\s*\{"
return_functions_pattern = r"(luaL_Reg\s*\*\s*(\w+)::GetFunctions\s*\(\s*\))\s*\{"
all_libs_pattern = r"(?:\#include\s+\"Libraries\/[\w\.]+\.hpp\"\s*\n)+\s*static\s+std::array<std::unique_ptr<Library>,\s*\d+>\s+allLibs\s*=\s*\{\s*(?:std::make_unique<\w+>\(\)\s*,?\s*)+\s*\n*\};"

all_libs = set()

def extract_full_function(content, start_pos):
    brace_count = 0
    function_start = content.find("{", start_pos)
    if function_start == -1:
        return None, None

    i = function_start
    while i < len(content):
        if content[i] == "{":
            brace_count += 1
        elif content[i] == "}":
            brace_count -= 1
            if brace_count == 0:
                return function_start, i + 1
        i += 1

    return None, None


for lib_path in all_libraries:
    with open(lib_path, "r", encoding="utf-8") as lib_file:
        lib_content = lib_file.read()

        custom_functions = re.findall(custom_func_pattern, lib_content)

        if not custom_functions:
            print(f"Found no custom functions in {lib_path}! Skipping...")
            continue

        return_functions_match = re.search(return_functions_pattern, lib_content)

        if not return_functions_match:
            print(f"Couldn't find return function declaration in {lib_path}! Skipping...")
            continue

        function_signature = return_functions_match.group(1)
        class_name = return_functions_match.group(2)

        function_start_pos = return_functions_match.start()
        function_body_start, function_body_end = extract_full_function(lib_content, function_start_pos)

        if function_body_start is None or function_body_end is None:
            print(f"Failed to extract full function body in {lib_path}. Skipping...")
            continue

        indentation = "     "
        new_function_body = f"""{indentation}const auto {class_name}_functions = new luaL_Reg[] {{
"""
        for func in custom_functions:
            new_function_body += f'{indentation}    {{"{func}", {func}}},\n'

        new_function_body += f"""{indentation}    {{nullptr, nullptr}}
{indentation}}};\n
{indentation}return {class_name}_functions;
}}
"""

        new_function_body = "\n".join(line.rstrip() for line in new_function_body.splitlines()) + "\n"
        updated_content = (
                lib_content[:function_body_start + 1].rstrip()
                + "\n"
                + new_function_body
                + lib_content[function_body_end:].lstrip()
        )

        with open(lib_path, "w", encoding="utf-8") as lib_file:
            lib_file.write(updated_content.rstrip() + "\n")

        all_libs.add(Path(lib_path))

        print(f"Updated {Path(lib_path).name}'s GetFunctions with {len(custom_functions)} custom function(s)!")

env_manager_path = source_dir / "Environment/EnvironmentManager.cpp"
with open(env_manager_path, "r", encoding="utf-8") as env_mng_file:
    env_mng_content = env_mng_file.read()
    env_mng_all_libs_match = re.search(all_libs_pattern, env_mng_content)

    if not env_mng_all_libs_match:
        print(f"Failed to find allLibs in env manager!")
        sys.exit(-1)

    new_includes = "\n".join([f'#include "Libraries/{lib.stem}.hpp"' for lib in sorted(all_libs)])
    std_make_unique_calls = ",\n    ".join(
        f"std::make_unique<{cls.stem}>()" for cls in sorted(all_libs, key=lambda x: x.stem)
    )
    new_all_libs = f"""
static std::array<std::unique_ptr<Library>, {len(all_libs)}> allLibs = {{
    {std_make_unique_calls}
}};
"""

    new_content = f"{new_includes}\n{new_all_libs}".strip()
    updated_content = re.sub(all_libs_pattern, new_content, env_mng_content, count=1)

    with open(env_manager_path, "w", encoding="utf-8") as env_mng_file:
        env_mng_file.write(updated_content)

    print("Updated EnvironmentManager.cpp to use all libs!")