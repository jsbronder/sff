# SFF

### What is this?
A terminal emulator that I'm
[dogfooding](http://www.catb.org/~esr/jargon/html/D/dogfood.html) daily.

### Ok, but why?
Because it's cool and that makes me cool.

### Really?
Honestly?  Yeah mostly, but also because I got a HiDPI laptop and was dismayed
to discover that every terminal emulator I found acceptable (didn't require me
to build a bunch of stuff I didn't already have) wouldn't scale fonts based on
the DPI of the current screen.  Given that the couple of Qt applications I use
like [qutebrowser](https://qutebrowser.org/) just worked, this seemed like a
reasonable direction.

### Why not...
First off, this is a pet project.  So, yeah, while I did look around a bit, I
really just wanted something to hack away on.  However, if you insist:
- [Anything libvte based](https://bugzilla.gnome.org/show_bug.cgi?id=679658#c10)
- [st](https://st.suckless.org/) would mean I'd have to maintain (or hope
  someone else did) a bunch of things I want as basic features.  I did mock up
  a font scaling patch
  [here](https://github.com/jsbronder/st/commit/4599b25263b8cb7ffd0dcd1a7672d92170600a2a).
- [Anything QTermWidget based](https://github.com/lxqt/qterminal/issues/320) looked
  like I'd be mucking with a lot of stuff libvterm solves already.
- [rxvt-unicode](http://software.schmorp.de/pkg/rxvt-unicode.html) is massive
  and I don't even know where to start trying to patch in dpi scaling.

### So what will get implemented?
At this point, just taking less than 100% of an entire core and not littering
artifacts all over the place is pretty sweet.  But if we're going to get
ambitious... Well, you should know I use vim.

Future:
- Visual mode for search and text selection.
- Position marks.
- Mouse double/triple click selection.
- Proper configuration of colors and what not using XDG\_CONFIG\_HOME.

Working-ish now:
- The stuff you'd expect from a basic terminal emulator.
- Behavior roughly equivalent to urxvt, st and pangoterm.
- Quick URL matching with copy to clipboard.
- Mouse scroll, selection and paste.

### Why libvterm?
It's good enough for neovim and I think they'll be around for a while.

But you vendored it?

Yeah, I know.  This will be one of the first things I'll get rid of if sff ever
goes beyond a toy project.


## Build and run
```
git clone https://github.com/jsbronder/sff
cd sff
git submodule update --init
mkdir b
cd b
cmake ..
make
bin/sff
```
