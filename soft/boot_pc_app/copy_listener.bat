set workspace=D:\03_Workspace\01_AMBO\Personal_Tracking\GT1022TrackerListener
set svn_workspace=GT1022TrackerListener
rmdir %svn_workspace% /S /Q
mkdir %svn_workspace%
xcopy %workspace%\* %svn_workspace%\* /e /exclude:exclude_list_listener.txt
set workspace=
set svn_workspace=
