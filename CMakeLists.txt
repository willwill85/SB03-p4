cmake_minimum_required(VERSION 3.16)

set(COMPONENTS main es8311 sb03_snore_prebuilt)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sb03-snore-customer-app)
