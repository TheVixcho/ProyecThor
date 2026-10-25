Name:           proyecthor
Version:        0.3.2
Release:        1%{?dist}
Summary:        Reproductor multimedia con VLC + OpenGL + ImGui
License:        Custom
URL:            https://github.com/tuusuario/proyecthor
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  git
BuildRequires:  pkgconfig(libvlc)
BuildRequires:  glfw-devel
BuildRequires:  mesa-libGL-devel

Requires:       vlc-libs
Requires:       glfw
Requires:       mesa-libGL
Recommends:     yt-dlp

%description
ProyecThor es un reproductor multimedia basado en libVLC para el
video/audio, OpenGL para el render y Dear ImGui para la interfaz.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -GNinja -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%files
%{_bindir}/proyecthor
%{_libdir}/proyecthor/
%{_datadir}/applications/proyecthor.desktop
%{_datadir}/icons/hicolor/256x256/apps/proyecthor.png

%changelog
* Mon Jul 06 2026 Tu Nombre <tu@email.com> - 0.3.2-1
- Version inicial empaquetada como .rpm
