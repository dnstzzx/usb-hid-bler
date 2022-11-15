set(srcs
    "src/parse.c"
    "src/translate.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "src/")