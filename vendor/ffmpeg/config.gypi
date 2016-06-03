{
  'variables': {
    'component': 'shared_library',
    'branding': 'Chromium',
    'chromeos': 0,
    'msan': 0,
    'clang': 0,

    'conditions': [
      ['OS=="win"', {
        'os_posix': 0,
        'win_use_allocator_shim': 0,
        'use_system_yasm': 0,
      }, {
        'os_posix': 1,
        'use_system_yasm': 1,
      }],
    ]
  },

  'target_defaults': {
    'msvs_cygwin_shell': 0,
    'default_configuration': 'Default',

    'xcode_settings': {
      'DEAD_CODE_STRIPPING': 'YES',  # -Wl,-dead_strip
      'GCC_OPTIMIZATION_LEVEL': 's',
    },

    'VCCLCompilerTool': {
      'RuntimeLibrary': '2', # 2 = /MD (nondebug DLL)
      'Optimization': '2',
      'WarnAsError': 'false',
      'AdditionalOptions': [
        '/d2Zi+',  # Improve debugging of Release builds.
      ],
    },

    'msvs_settings': {
      'VCLinkerTool': {
        # LinkIncremental is a tri-state boolean, where 0 means default
        # (i.e., inherit from parent solution), 1 means false, and
        # 2 means true.
        'LinkIncremental': '1',
        # This corresponds to the /PROFILE flag which ensures the PDB
        # file contains FIXUP information (growing the PDB file by about
        # 5%) but does not otherwise alter the output binary. This
        # information is used by the Syzygy optimization tool when
        # decomposing the release image.
        'Profile': 'true',
        'AdditionalDependencies': [
          'advapi32.lib',
        ]
      },
    },

    'conditions': [
      ['target_arch=="ia32"', {
        'msvs_settings': {
          'VCLinkerTool': {
            'SubSystem': '1',
            'MinimumRequiredVersion': '5.01',  # XP.
            'TargetMachine': '1',
          },
          'VCLibrarianTool': {
            'TargetMachine': '1',
          },
        },
        'configurations': {
          'Default': {
            'msvs_configuration_platform': 'x86'
          }
        }
      }, {
        'msvs_settings': {
          'VCLinkerTool': {
            'SubSystem': '1',
            'MinimumRequiredVersion': '5.02',  # Server 2003.
            'TargetMachine': '17', # x86 - 64
          },
          'VCLibrarianTool': {
            'TargetMachine': '17', # x64
          },
        },
        'configurations': {
          'Default': {
            'msvs_configuration_platform': 'x64'
          },
          'Default_x64': {
            'msvs_configuration_platform': 'x64'
          }
        }
      }]
    ],
  }
}
