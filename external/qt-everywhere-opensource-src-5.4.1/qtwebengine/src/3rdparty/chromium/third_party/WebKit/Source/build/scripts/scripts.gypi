# The GN build definitions for these variables are in scripts.gni.
{
    'variables': {
        'scripts_for_in_files': [
            # jinja2/__init__.py contains version string, so sufficient as
            # dependency for whole jinja2 package
            '<(DEPTH)/third_party/jinja2/__init__.py',
            '<(DEPTH)/third_party/markupsafe/__init__.py',  # jinja2 dep
            'hasher.py',
            'in_file.py',
            'in_generator.py',
            'license.py',
            'name_macros.py',
            'name_utilities.py',
            'template_expander.py',
            'templates/macros.tmpl',
        ],
        'make_event_factory_files': [
            '<@(scripts_for_in_files)',
            'make_event_factory.py',
            'templates/EventFactory.cpp.tmpl',
        ],
        'make_names_files': [
            '<@(scripts_for_in_files)',
            'make_names.py',
            'templates/MakeNames.cpp.tmpl',
            'templates/MakeNames.h.tmpl',
        ],
        'make_qualified_names_files': [
            '<@(scripts_for_in_files)',
            'make_qualified_names.py',
            'templates/MakeQualifiedNames.cpp.tmpl',
            'templates/MakeQualifiedNames.h.tmpl',
        ],
        'make_element_factory_files': [
            '<@(make_qualified_names_files)',
            'make_element_factory.py',
            'templates/ElementFactory.cpp.tmpl',
            'templates/ElementFactory.h.tmpl',
            'templates/ElementWrapperFactory.cpp.tmpl',
            'templates/ElementWrapperFactory.h.tmpl',
        ],
        'make_element_type_helpers_files': [
            '<@(make_qualified_names_files)',
            'make_element_type_helpers.py',
            'templates/ElementTypeHelpers.h.tmpl',
        ],
        'conditions': [
            ['OS=="win"', {
                # Using native perl rather than cygwin perl cuts execution time
                # of idl preprocessing rules by a bit more than 50%.
                'perl_exe%': '<(DEPTH)/third_party/perl/perl/bin/perl.exe',
                'gperf_exe%': '<(DEPTH)/third_party/gperf/bin/gperf.exe',
                'bison_exe%': '<(DEPTH)/third_party/bison/bin/bison.exe',
                # Using cl instead of cygwin gcc cuts the processing time from
                # 1m58s to 0m52s.
                'preprocessor': '--preprocessor "cl.exe -nologo -EP -TP"',
              },{
                'perl_exe': 'perl',
                'gperf_exe': 'gperf',
                'bison_exe': 'bison',
                # We specify a preprocess so it happens locally and won't get
                # distributed to goma.
                # FIXME: /usr/bin/gcc won't exist on OSX forever. We want to
                # use /usr/bin/clang once we require Xcode 4.x.
                'preprocessor': '--preprocessor "/usr/bin/gcc -E -P -x c++"'
              }],
         ],
    },
}
