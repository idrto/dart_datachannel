Pod::Spec.new do |s|
  s.name             = 'flutter_datachannel'
  s.version          = '0.1.0'
  s.summary          = 'WebRTC data channels for Flutter via libdatachannel'
  s.description      = <<-DESC
Thin FFI layer over libdatachannel for Flutter. Supports client, server, and hybrid modes.
                       DESC
  s.homepage         = 'https://github.com/idrto/flutter_datachannel'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'idrto' => 'https://github.com/idrto' }
  s.source           = { :path => '.' }
  s.dependency 'Flutter'
  s.platform = :ios, '13.0'
  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
  }
  s.swift_version = '5.0'
  # Native C++ is built via Flutter's FFI plugin CMake hook (ios/CMakeLists.txt).
end
