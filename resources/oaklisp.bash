# Bash completion for oaklisp.
#
# Emulator options come before "--", Oaklisp options after it.  File
# arguments to --load and --compile are given without their .oak/.oa
# extension, so those are offered as base names.

_oaklisp()
{
    local cur prev words cword split
    _init_completion -s || return

    local emulator_opts="--help --version --world --dump --dump-base
        --predump-gc --size-heap --size-val-stk --size-cxt-stk
        --size-seg-max --trace-gc --verbose-gc --trace-traps --batch
        --no-line-editing --trace-files --trace-segs --trace-valcon
        --trace-cxtcon --trace-stks --trace-instructions --trace-methods
        --trace-mcache --"
    local oaklisp_opts="--help --version --eval --load --compile --outdir
        --target --locale --pthreads --exit"

    # Which side of "--" are we on?
    local i after=0
    for ((i = 1; i < cword; i++)); do
        [[ ${words[i]} == -- ]] && after=1 && break
    done

    case $prev in
        --world|--dump|-d)
            _filedir bin
            return ;;
        --outdir)
            _filedir -d
            return ;;
        --load|--compile)
            local f names=()
            for f in "$cur"*.oak "$cur"*.oa; do
                [[ -e $f ]] || continue
                f=${f%.oak}; names+=("${f%.oa}")
            done
            COMPREPLY=($(printf '%s\n' "${names[@]}" | sort -u))
            _filedir -d
            return ;;
        --target)
            COMPREPLY=($(compgen -W "bc2-32 bc2-64 bc4-64" -- "$cur"))
            return ;;
        --locale)
            COMPREPLY=($(compgen -W "system-locale compiler-locale
                scheme-locale user-locale oaklisp-locale" -- "$cur"))
            return ;;
        --dump-base|--predump-gc|--size-heap|--size-val-stk|--size-cxt-stk|\
        --size-seg-max|--trace-gc|--verbose-gc|--pthreads|--eval)
            return ;;
    esac

    if [[ $cur == -* ]]; then
        if ((after)); then
            COMPREPLY=($(compgen -W "$oaklisp_opts" -- "$cur"))
        else
            COMPREPLY=($(compgen -W "$emulator_opts" -- "$cur"))
        fi
    fi
} &&
    complete -F _oaklisp oaklisp
