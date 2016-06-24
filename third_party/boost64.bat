pushd boost

pushd tools\build
call bootstrap.bat
b2 install --prefix=..\..\boost_build
popd
set PATH=%CD%\boost_build\bin;%PATH%

b2 --build-dir=build64 toolset=msvc -j 8 variant=debug,release link=static threading=multi address-model=64 runtime-link=shared --with-test --build-type=complete --stagedir=stage-x64 stage
popd
