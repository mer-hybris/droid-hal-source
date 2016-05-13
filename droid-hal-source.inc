# This file should be %%included into a device specific spec file
# where macros are defined:
# device: should be the CM codename or the AOSP TARGET_PRODUCT
# vendor: determine the directory used for ./device/<vendor>/<device>
# device_pretty: User-visible model name of the device
# vendor_pretty: User-visible manufacturer name of the device
# hadk_make_target: the target used when running make in the HABUILD_SDK on the OBS. Defaults to "hybris-hal"
# device_target_cpu: Used for native builds. Normally the nested droid build env will do an old-fashioned cross-compile and produce non-x86 binaries (default armv7hl). This can be set to tell OBS what arch the binaries are. Eg Android for Intel arch must set this.
# device_variant: for AOSP this is used as the TARGET_BUILD_VARIANT for lunch

%define __provides_exclude_from ^%{_libexecdir}/droid-hybris/.*$
%define android_root .

%define __find_provides %{nil}
%define __find_requires %{nil}
%define __strip /bin/true
%define __provides_exclude_from ^/system/.*$
%define __requires_exclude ^.*$
%global debug_package %{nil}

# On the OBS this package should be built in the i486 scheduler against
# mer/sailfish *_i486 targets.
# The prjconf should have an ExportFilter like this (mer/sailfish has this):
#   ExportFilter: \.armv7hl\.rpm$ armv8el
# We lie about our architecture and allows OBS to cross-publish this 486 cross-built spec to the armv7hl repos
%if 0%{?device_target_cpu:1}
%define _target_cpu %{device_target_cpu}
%else
%define _target_cpu armv7hl
%endif

# Support build info extracted from OBS builds too
%if 0%{?_obs_build_project:1}
%define _build_flavour %(echo %{_obs_build_project} | awk -F : '{if ($NF == "testing" || $NF == "release") print $NF; else if ($NF ~ /[0-9]\.[0-9]\.[0-9]/ && NF == 3) print strdevel; else if (NF == 2) print strdevel; else print strunknown}' strdevel=devel strunknown=unknown)
%else
%define _build_flavour unknown
%endif

%define _obs_build_count %(echo %{release} | awk -F . '{if (NF >= 3) print $3; else print $1 }')
%define _obs_commit_count %(echo %{release} | awk -F . '{if (NF >= 2) print $2; else print $1 }')

%if "%{_build_flavour}" == "release"
%define _version_appendix (%{_target_cpu})
%else
%define _version_appendix (%{_target_cpu},%{_build_flavour})
%endif

%if 0%{?ha_device_override:1}
%define ha_device %{ha_device_override}
%else
%define ha_device %{device}
%endif

# Don't run strip
%define __strip /bin/true

Summary: 	Droid BIN package for %{ha_device}
License: 	BSD-3-Clause
Name: 		droid-bin-%{ha_device}
Version: 	0.0.6
# timestamped releases are used only for HADK (mb2) builds
%if 0%{?_obs_build_project:1}
Release:	1
%else
Release:	%(date +'%%Y%%m%%d%%H%%M')
%endif
Provides:	droid-bin
# The repo sync service on OBS prepares a 'source tarball' of the rpm
# dir since we currently have a complex setup with subdirs which OBS
# doesn't like. This is not a problem for local builds.
Source0: 	rpm.tar.bzip2
# Ths actual droid source from the repo service when run on OBS.
# local builds don't mind if this is missing
Source40:       repo.tar.bzip2
# Reserve Source50 onwards
# Allow device specific sources to be defined using dbd_sources
%{?dbd_sources}

Group:		System

# Ignore the rpmlint-* to avoid long RPMLINT reporting
#!BuildIgnore: rpmlint-mini
#!BuildIgnore: rpmlint-MeeGo
# Don't run any of the MeeGo brp-strip-* or other install_post validation commands
%define __os_install_post %{nil}

%description
%{summary}.

%if 0%{?dbd_src_trees:1}
%include package-section
%endif

%prep
# No %%setup macro !!

%if 0%{?_obs_build_project:1}
# The OBS does not have access to 'repo' so a service does the repo init/sync
# and provides a (huge) tarball with the checked-out tree in it.
# So now drop to android_root and pretend to do a repo sync
tar xf %{SOURCE40} -C %android_root
# Clean up the repo tarball to save space
rm -f %{SOURCE40}
# Make a dummy tarball for rpm checks
mkdir dummy;(cd dummy; touch dummy; tar cvf - . | bzip2 > %{SOURCE40}); rm -rf dummy
# unpack the directories to SOURCES ... this needs to change
tar xf %{SOURCE0} -C ../SOURCES
# Clean up the rpm tarball too
rm -f %{SOURCE0}
cp %{SOURCE40} %{SOURCE0}

# In OBS the repo service leaves the rpm/* files for OBS and they just ^^
# got unpacked to ../SOURCES ... but we're used to having an rpm/ dir
# So if rpm/ is missing then we use ../SOURCES :
[ -d rpm ] || ln -s ../SOURCES rpm
%endif

%build

echo _target_cpu is %{_target_cpu}

# We'll hardcode the device/variant information into the droid-make
# script This isn't trivially installable into the ubu-chroot so
# include the ubu-chroot command within it
cat <<"EOF" > droid-make
#!/bin/bash

# This command runs a hardware-specific 'make' command inside the
# ubu-chroot with the correct lunch setup
# It is only intended to run in the OBS builders

exec ubu-chroot -r /srv/mer/sdks/ubu "%{?pre_actions}; source build/envsetup.sh; lunch %{device}%{?device_variant}; make $*"
EOF

################
%install
rm -rf $RPM_BUILD_ROOT

# Support the building of src-* rpms and srcutils if they're wanted
%if 0%{?dbd_src_trees:1}
# To create a set of rpms that hold the *source* we move the subset of
# src to the buildroot for packaging
# These will be used to create buildroots for packages like droidmedia
mkdir -p $RPM_BUILD_ROOT/home/abuild/src/droid
for tree in %dbd_src_trees ; do
   d=$(dirname $tree)
   mkdir -p $RPM_BUILD_ROOT/home/abuild/src/droid/$d
   mv %android_root/$tree $RPM_BUILD_ROOT/home/abuild/src/droid/$d
done

# Top level makefile
mv %android_root/Makefile $RPM_BUILD_ROOT/home/abuild/src/droid/

# Install the droid-make helper
mkdir -p $RPM_BUILD_ROOT/usr/bin
cp droid-make $RPM_BUILD_ROOT/usr/bin

rm -rf tmp/
mkdir tmp/

find %android_root -maxdepth 1 \( -type f -or -type l \) -print -exec mv {} $RPM_BUILD_ROOT/home/abuild/src/droid/ \; | sed 's ^%android_root /home/abuild/src/droid ' >> tmp/rootdir.files

%endif

################################################################
# Begin files section

#files
#defattr(-,root,root,-)

%if 0%{?dbd_src_trees:1}
%include files-section
%endif
