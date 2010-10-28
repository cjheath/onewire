require 'mkmf'

printf("checking for OS... ")
STDOUT.flush
os = /-([a-z]+)/.match(RUBY_PLATFORM)[1]
puts(os)
$CFLAGS += " -D#{os} -Iowpd300/common"

if !(os == 'mswin' or os == 'bccwin')
  exit(1) if not have_header("termios.h") or not have_header("unistd.h")
end

$DLDFLAGS += libpathflag("owpd300") 

if (os == 'mswin' or os == 'bccwin')
    $LIBS += " libcommon.#{$LIBEXT} libuserial.#{$LIBEXT}"
else
    $LIBS += " -lcommon -luserial"
end

create_makefile("onewire")
