# usage: $ perl watcher.pl watch_dir
use common::sense;
use Filesys::Notify::Simple;

my $watcher = Filesys::Notify::Simple->new([ $ARGV[0] ]);
while (1) {
    $watcher->wait(
        sub {
            for my $event (@_) {
                say `./ficiauploader -f $event->{path}`;
            }
        }
    );
}
