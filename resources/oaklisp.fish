# Fish completion for oaklisp.  Emulator options come before "--",
# Oaklisp options after it.  --load and --compile take file names
# without their .oak/.oa extension.

function __oaklisp_after_dashdash
    set -l tokens (commandline -opc)
    contains -- -- $tokens
end

function __oaklisp_oak_files
    for f in *.oak *.oa
        string replace -r '\.oak?$' '' -- $f
    end
end

complete -c oaklisp -f

# Emulator options
set -l before 'not __oaklisp_after_dashdash'
complete -c oaklisp -n $before -l help -d 'show help'
complete -c oaklisp -n $before -l version -d 'show version'
complete -c oaklisp -n $before -l world -r -F -d 'world image to load'
complete -c oaklisp -n $before -l dump -r -F -d 'dump the world to file on exit'
complete -c oaklisp -n $before -l dump-base -x -d 'base of the numbers in a dumped world'
complete -c oaklisp -n $before -l predump-gc -x -d 'garbage collect before dumping'
complete -c oaklisp -n $before -l size-heap -x -d 'heap size in kilo-refs'
complete -c oaklisp -n $before -l size-val-stk -x -d 'value stack buffer, in refs'
complete -c oaklisp -n $before -l size-cxt-stk -x -d 'context stack buffer, in refs'
complete -c oaklisp -n $before -l size-seg-max -x -d 'maximum flushed segment length, in refs'
complete -c oaklisp -n $before -l trace-gc -x -a '0 1 2 3' -d 'trace garbage collection'
complete -c oaklisp -n $before -l verbose-gc -x -a '0 1 2 3' -d 'trace garbage collection'
complete -c oaklisp -n $before -l trace-traps -d 'trace tag traps'
complete -c oaklisp -n $before -l batch -d 'disable trapping of SIGINT'
complete -c oaklisp -n $before -l no-line-editing -d 'read the terminal without GNU readline'
complete -c oaklisp -n $before -l trace-files -d 'trace filesystem operations'
complete -c oaklisp -n $before -l trace-segs -d 'trace stack segment writes/reads'
complete -c oaklisp -n $before -l trace-valcon -d 'print the value stack at each instruction'
complete -c oaklisp -n $before -l trace-cxtcon -d 'print the context stack at each instruction'
complete -c oaklisp -n $before -l trace-stks -d 'print the stack sizes at each instruction'
complete -c oaklisp -n $before -l trace-instructions -d 'trace each bytecode executed'
complete -c oaklisp -n $before -l trace-methods -d 'trace each method lookup'
complete -c oaklisp -n $before -l trace-mcache -d 'trace the method cache'

# Oaklisp options
set -l after '__oaklisp_after_dashdash'
complete -c oaklisp -n $after -l help -d 'show help'
complete -c oaklisp -n $after -l version -d 'show version'
complete -c oaklisp -n $after -l eval -x -d 'evaluate an expression'
complete -c oaklisp -n $after -l load -x -a '(__oaklisp_oak_files)' -d 'load a file'
complete -c oaklisp -n $after -l compile -x -a '(__oaklisp_oak_files)' -d 'compile file.oak to file.oa'
complete -c oaklisp -n $after -l outdir -x -a '(__fish_complete_directories)' -d 'directory for later --compile output'
complete -c oaklisp -n $after -l target -x -a 'bc2-32 bc2-64 bc4-64' -d 'architecture to compile for'
complete -c oaklisp -n $after -l locale -x -a 'system-locale compiler-locale scheme-locale user-locale oaklisp-locale' -d 'switch to a locale'
complete -c oaklisp -n $after -l pthreads -x -d 'spawn N native threads'
complete -c oaklisp -n $after -l exit -d 'exit after processing this option'
