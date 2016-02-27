set workspace=D:\03_Workspace\01_AMBO\Personal_Tracking\GT1022Firmware\build
set svn_workspace=build
rmdir %svn_workspace% /S /Q
mkdir %svn_workspace%
xcopy %workspace%\* %svn_workspace%\* /e /exclude:exclude_list.txt

set workspace=D:\03_Workspace\01_AMBO\Personal_Tracking\GT1022Firmware\src
set svn_workspace=src
rmdir %svn_workspace% /S /Q
mkdir %svn_workspace%
xcopy %workspace%\* %svn_workspace%\* /e

set workspace=
set svn_workspace=

