# Enforces the dependency rule: core is not allowed to include the view layer.
# Run via: cmake -DCHECK_ROOT=<src dir> -P tools/check_dependency_rule.cmake

if(NOT DEFINED CHECK_ROOT)
  message(FATAL_ERROR "CHECK_ROOT is not set")
endif()

set(guarded_dirs core resources)

set(violations "")
foreach(dir IN LISTS guarded_dirs)
  file(GLOB_RECURSE files "${CHECK_ROOT}/${dir}/*.h" "${CHECK_ROOT}/${dir}/*.cpp")
  foreach(file IN LISTS files)
    file(STRINGS "${file}" hits REGEX "#[ ]*include[ ]*\"views/")
    if(hits)
      file(RELATIVE_PATH rel "${CHECK_ROOT}" "${file}")
      foreach(hit IN LISTS hits)
        string(STRIP "${hit}" hit)
        list(APPEND violations "  ${rel}: ${hit}")
      endforeach()
    endif()
  endforeach()
endforeach()

if(violations)
  string(REPLACE ";" "\n" violations "${violations}")
  message(FATAL_ERROR
    "Dependency rule violation: a core directory includes the view layer:\n"
    "${violations}\n\n")
endif()

message(STATUS "OK")
