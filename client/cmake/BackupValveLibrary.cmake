# Keeps Valve's original library beside the replacement as <name>.valve.
function(BackupValveLibrary path)
    if (EXISTS "${path}" AND NOT EXISTS "${path}.valve")
        file(RENAME "${path}" "${path}.valve")
    endif ()
endfunction()
