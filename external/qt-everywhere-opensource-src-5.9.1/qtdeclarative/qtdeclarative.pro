CONFIG += tests_need_tools examples_need_tools
load(qt_parts)

!python_available {
    py_out = $$system('python -c "print(1)"')
    !equals(py_out, 1): error("Building QtQml requires Python.")
    tmp = python_available
    CONFIG += $$tmp
    cache(CONFIG, add, tmp)
}
