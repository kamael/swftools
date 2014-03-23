
# ======================================================#
# File automagically generated by GUI2Exe version 0.3
# Andrea Gavana, 01 April 2007
# ======================================================#

# Let's start with some default (for me) imports...

from distutils.core import setup
import py2exe
import glob
import os
import zlib
import shutil

from distutils.core import setup
import py2exe,sys,os

# Overriding Criteria for Including DLLs, see:
# http://www.py2exe.org/index.cgi/OverridingCriteraForIncludingDlls
# XXX: Check the permission to redistribute msvcp71.dll
origIsSystemDLL = py2exe.build_exe.isSystemDLL
def isSystemDLL(pathname):
        if os.path.basename(pathname).lower() in ("msvcp71.dll",):
                return 0
        return origIsSystemDLL(pathname)
py2exe.build_exe.isSystemDLL = isSystemDLL


# Remove the build folder
shutil.rmtree("build", ignore_errors=True)


manifest_template = """
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
<assemblyIdentity
    version="5.0.0.0"
    processorArchitecture="x86"
    name="gpdf2swf"
    type="win32"
/>
<description>gpdf2swf Program</description>
<dependency>
    <dependentAssembly>
        <assemblyIdentity
            type="win32"
            name="Microsoft.Windows.Common-Controls"
            version="6.0.0.0"
            processorArchitecture="X86"
            publicKeyToken="6595b64144ccf1df"
            language="*"
        />
    </dependentAssembly>
</dependency>
</assembly>
"""

class Target(object):
    """ A simple class that holds information on our executable file. """
    def __init__(self, **kw):
        """ Default class constructor. Update as you need. """
        self.__dict__.update(kw)
        

# Ok, let's explain why I am doing that.
# Often, data_files, excludes and dll_excludes (but also resources)
# can be very long list of things, and this will clutter too much
# the setup call at the end of this file. So, I put all the big lists
# here and I wrap them using the textwrap module.

data_files = [('viewers', ['viewers/__init__.py',
                           'viewers/raw.py',
                           'viewers/rfx.py',
                           'viewers/rfx7.swf',
                           'viewers/rfx8.swf',
                           'viewers/simple.py']),
              #('.', ['dll/msvcp71.dll',
              #      ]),
              #('images', ['images/stop.png',
              #            'images/pdf2swf_gui.ico'])
             ]

includes = []
excludes = ['_gtkagg', '_tkagg', 'bsddb', 'curses', 'email', 'pywin.debugger',
            'pywin.debugger.dbgcon', 'pywin.dialogs', 'tcl',
            'Tkconstants', 'Tkinter']
packages = ['viewers']
dll_excludes = ['libgdk-win32-2.0-0.dll', 'libgobject-2.0-0.dll', 'tcl84.dll',
                'tk84.dll']
icon_resources = [(1, 'gpdf2swf.ico')]
bitmap_resources = []
other_resources = [(24, 1, manifest_template)]


# This is a place where the user custom code may go. You can do almost
# whatever you want, even modify the data_files, includes and friends
# here as long as they have the same variable name that the setup call
# below is expecting.

# No custom code added


# Ok, now we are going to build our target class.
# I chose this building strategy as it works perfectly for me :-D


GUI2Exe_Target_1 = Target(
    # what to build
    script = "gpdf2swf.py",
    icon_resources = icon_resources,
    bitmap_resources = bitmap_resources,
    other_resources = other_resources,
    dest_base = "gpdf2swf",    
    version = "0.9.0",
    company_name = "swftools",
    copyright = "swftools",
    name = "gpdf2exe"
    )



# That's serious now: we have all (or almost all) the options py2exe
# supports. I put them all even if some of them are usually defaulted
# and not used. Some of them I didn't even know about.

setup(

    data_files = data_files,

    options = {"py2exe": {"compressed": 0, 
                          "optimize": 2,
                          "includes": includes,
                          "excludes": excludes,
                          "packages": packages,
                          "dll_excludes": dll_excludes,
                          "bundle_files": 3,
                          "dist_dir": "dist",
                          "xref": False,
                          "skip_archive": False,
                          "ascii": False,
                          "custom_boot_script": '',
                         }
              },

    zipfile = None,
    console = [],
    windows = [GUI2Exe_Target_1]
    )

# This is a place where any post-compile code may go.
# You can add as much code as you want, which can be used, for example,
# to clean up your folders or to do some particular post-compilation
# actions.

# No post-compilation code added


# And we are done. That's a setup script :-D

# Run setup standalone...
if __name__ == "__main__":
    setup()
