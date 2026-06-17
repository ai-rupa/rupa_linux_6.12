if (RKAIQ_HAVE_PDAF)
set(PDLIB_SONY_SRC
    PdafLibrary.c
    PdafMathFunc.c
    )

dec_algo(pdlibsony PdafLibrary.c ${PDLIB_SONY_SRC})

endif()

