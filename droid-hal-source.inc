# This file should be %%included into a device specific spec file
# where macros are defined:
# device:            should be the CM codename or the AOSP TARGET_PRODUCT
# hadk_make_target:  the target used when running make in the HABUILD_SDK on the
#                      OBS. Defaults to "hybris-hal"
# device_variant:    for AOSP this is used as the TARGET_BUILD_VARIANT for lunch
# lunch_device:      cases where the lunch combo is different from device name.
#                      For example, it's "aosp_f5121" for the "suzu" device
# have_vendor_src_for_obs:
#                    include a separately packaged vendor source for OBS builds
# post_build_actions:
#                    Actions to be executed after the sources have been extracted
#                    for example patches can be applied to the sources which are
#                    then stored in the final rpm droid-src packages.


%define __provides_exclude_from ^%{_libexecdir}/droid-hybris/.*$
%define android_root .

%define __find_provides %{nil}
%define __find_requires %{nil}
%define __strip /bin/true
%define __provides_exclude_from ^/system/.*$
%define __requires_exclude ^.*$
%global debug_package %{nil}

# Support build info extracted from OBS builds too
%if 0%{?_obs_build_project:1}
%define _build_flavour %(echo %{_obs_build_project} | awk -F : '{if ($NF == "testing" || $NF == "release") print $NF; else if ($NF ~ /[0-9]\.[0-9]\.[0-9]/ && NF == 3) print strdevel; else if (NF == 2) print strdevel; else print strunknown}' strdevel=devel strunknown=unknown)
%else
%define _build_flavour unknown
%endif

%define _obs_build_count %(echo %{release} | awk -F . '{if (NF >= 3) print $3; else print $1 }')
%define _obs_commit_count %(echo %{release} | awk -F . '{if (NF >= 2) print $2; else print $1 }')

# We build noarch packages and some sources includes binaries as well
%define _binaries_in_noarch_packages_terminate_build 0

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

%if 0%{?dhs_flavour:1}
%define dhs_feature droid-src-%{dhs_flavour}
%else
%define dhs_feature droid-src
%endif

%define dhs_name %{dhs_feature}-%{ha_device}

# if dhs_flavour is not defined, means we're building a generic droid-src, and
# for backwards compatibility we'll need to provide droid-bin and others
%if 0%{?dhs_flavour:1}
%define dhs_legacy 0
%else
%define dhs_legacy 1
%endif

# Don't run strip
%define __strip /bin/true

Summary: 	Droid SRC package for %{ha_device}%{?dhs_flavour:, %{dhs_flavour} flavour}
License: 	BSD
Name: 		%{dhs_name_hardcoded}
Version: 	0.0.0.1
# timestamped releases are used only for HADK (mb2) builds
%if 0%{?_obs_build_project:1}
Release:	1
%else
Release:	%(date +'%%Y%%m%%d%%H%%M')
%endif
Provides:	%{dhs_feature}
%if %{dhs_legacy}
Provides:	droid-bin
%endif
# The repo sync service on OBS prepares a 'source tarball' of the rpm
# dir since we currently have a complex setup with subdirs which OBS
# doesn't like. This is not a problem for local builds.
Source0: 	rpm.tar.bzip2
# Ths actual droid source from the repo service when run on OBS.
# local builds don't mind if this is missing
Source40:       repo.tar.bzip2
# Reserve Source50 onwards
# Allow device specific sources to be defined using dhs_sources
%{?dhs_sources}

Group:		System

# droid-src does not build any binaries, just bundles sources
BuildArch:      noarch

%if 0%{?_obs_build_project:1}
%if 0%{?have_vendor_src_for_obs:1}
BuildRequires:  droid-system-vendor-obsbuild
%endif
%endif

# Ignore the rpmlint-* to avoid long RPMLINT reporting
#!BuildIgnore: rpmlint-mini
#!BuildIgnore: rpmlint-MeeGo
# Don't run any of the MeeGo brp-strip-* or other install_post validation commands
%define __os_install_post %{nil}

%description
%{summary}.

%if 0%{?dhs_trees:1}
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

%if 0%{?have_vendor_src_for_obs:1}
# Copy SW binaries to the build dir (provided by droid-system-vendor-obsbuild)
cp -ar /vendor .
%endif

# In OBS the repo service leaves the rpm/* files for OBS and they just ^^
# got unpacked to ../SOURCES ... but we're used to having an rpm/ dir
# So if rpm/ is missing then we use ../SOURCES :
[ -d rpm ] || ln -s ../SOURCES rpm
%endif

%build

# We'll hardcode the device/variant information into the droid-make
# script This isn't trivially installable into the ubu-chroot so
# include the ubu-chroot command within it
cat <<"EOF" > droid-make
#!/bin/bash

# This command runs a hardware-specific 'make' command inside the
# ubu-chroot with the correct lunch setup
# It is only intended to run in the OBS builders

# We can check if we have new or old ubu-chroot by checking if it has the -V  option
# added with this version.
if ubu-chroot -V ; then
   bash="bash -c"
fi

exec ubu-chroot -r /srv/mer/sdks/ubu ${bash} "set -o errexit; %{?pre_actions}; source build/envsetup.sh; lunch %{?lunch_device}%{!?lunch_device:%{device}}%{?device_variant}; make $*"
EOF

%{?post_build_actions}

################
%install
rm -rf $RPM_BUILD_ROOT

# Support the building of src-* rpms and srcutils if they're wanted
%if 0%{?dhs_trees:1}
# To create a set of rpms that hold the *source* we move the subset of
# src to the buildroot for packaging
# These will be used to create buildroots for packages like droidmedia
mkdir -p $RPM_BUILD_ROOT/home/abuild/src/droid
for tree in %dhs_trees ; do
   d=$(dirname $tree)
   mkdir -p $RPM_BUILD_ROOT/home/abuild/src/droid/$d
   mv %android_root/$tree $RPM_BUILD_ROOT/home/abuild/src/droid/$d
done

# Top level makefile
if [ -f %android_root/Makefile ]; then
mv %android_root/Makefile $RPM_BUILD_ROOT/home/abuild/src/droid/
fi

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

%if 0%{?dhs_trees:1}
%include files-section
%endif
