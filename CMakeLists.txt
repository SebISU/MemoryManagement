#
# DANTE :: System Automatycznych Testów Jednostkowych oraz Akceptacyjnych
# Tomasz Jaworski, 2018-2020
#
# Plik wygenerowany automatycznie
# Znacznik czasowy: 2020-10-26 17:15:10.431208
#

cmake_minimum_required(VERSION 3.17)
project(project1 C)

# Przyjmij standard C11
set(CMAKE_C_STANDARD 11)

# Ustaw opcje kompilatora (z raportu Dante)
add_compile_options(
      "-Werror=vla"
      "-pedantic"
      "-DINSIDE_DANTE"
      "-Werror"
      "-fdiagnostics-color"
      "-Wextra"
      "-fmax-errors=5"
      "-Wno-error=unused-parameter"
      "-D_ANSI_OUTPUT"
      "-Wno-error=implicit-fallthrough"
      "-Wall"
      "-Wno-parentheses"
      "-D_GNU_SOURCE"
      "-ggdb3"
      "-xc"
      "-Wno-error=parentheses"
      "-std=c11"

)

# Ustaw opcje konsolidatora (z raportu Dante)
add_link_options(
        "-Wl,-Map=main.map"
        "-Wl,-cref"
        "-Wl,-wrap,main"
        "-ggdb3"
)

# Utwórz target 'project1' i dodaj do niego pliki źródlowe:
add_executable(project1
        "heap.c"
        "main.c"
        "unit_helper_v2.c"
        "unit_test_v2.c"
        "rdebug.c"
        "memmanager.c"
)

# Dołącz niezbędne biblioteki
target_link_libraries(project1
        "m"
        "pthread"
)