## sdiol: Software-Defined IO for Linux

`sdiol` (pronounced: "style") extends standard IO devices, like keyboards and
mice, with sophisticated features like multi-layered keymaps and network
serving.  Possible use-cases include:

* Converting caps lock into a control key.

* Configuring left shift key to be a shift key when held or an escape key when
tapped.

* Configuring the f key to be an f when tapped or to convert the h/j/k/l
keys to behave like arrow keys while f is held.

* Configure multiple keyboards with different keybindings.

* Share a keyboard and/or mouse between multiple desktop/laptop computers over
the network so you don't have to constantly plug and unplug devices or keep
multiple keyboards and/or mice on your desk.

`sdiol` operates at the device level, so it works equally well in tty, X
windows, or Wayland environments.

## User Guide

See section on `Building` (below) if you want to follow along locally.

Here is the output of `sdiol --help`:

    usage: sdiol local                  # modify local IO
    usage: sdiol serve unix_socket      # serve IO over a unix socket
    usage: sdiol read                   # read IO from STDIN

    # insecure, experimental features:
    usage: sdiol serve-tcp [host] port  # serve IO over the network
    usage: sdiol connect host port      # read IO from the network

    general options:
     -h, --help           print this help text
     -c, --config FILE    set config file (default /etc/sdiol/conf.lua)
     -v, --verbose        print useful info while running
         --timeout N      exit after N seconds (for testing)
         --systemd        run as systemd Type=notify service

    options specific to sdiol serve:
     --chown-socket USER:GROUP  set user and group of unix socket
     --chmod-socket MODE        set mode of unix socket, e.g. 600

`sdiol` requires a Lua configuration file to run (see `Configuration Reference`,
below, for details).  By default it looks for `/etc/sdiol/conf.lua`, but we
can also specify a file with the `--config` option.

So let's start by creating a file called `conf.lua` with the following content:

    -- the f key will expose an alternate layer of the keymap when held
    f_keymap = {
        -- when f is held, convert h/j/k/l to arrow keys
        KEY_H = KEY_LEFT,
        KEY_J = KEY_DOWN,
        KEY_K = KEY_UP,
        KEY_L = KEY_RIGHT,
        -- also convert u and i to left and right parentheses
        KEY_U = shift(KEY_9),
        KEY_I = shift(KEY_0),
    }

    root_keymap = {
        -- caps lock is way too useless of a key for your home row
        KEY_CAPSLOCK = KEY_LEFTCTRL,
        -- assign f to have different behavior on tap vs hold
        KEY_F = dual_key(KEY_F, f_keymap),
    }

    -- use a case-insensitive regex to identify the keyboard device(s) to grab
    grab_keyboard(".*keyboard.*", root_keymap)

Now we will do a test run:

    sudo ./sdiol local --config conf.lua --timeout 20 --verbose

The argument meanings are as follows:

* `sudo` is required because we are trying to claim hardware devices

* `local` means we are intercepting a local keyboard

* `--timeout 20` means "exit after 20 seconds", so if we lose the ability to
type `ctrl-c`, we don't have to reboot the computer

* `--verbose` will help us debug our config by printing key names after each key
press and by printing device names at startup or after a new device is plugged
in

If you don't see a line like `grabbing <your keyboard name>` you may have to
edit the regex in `conf.lua` before continuing.

Now, while the `sdiol` process is running, you can open a text editor and
verify the following behaviors:

* The caps lock key has become a control key

* Tapping f types an f (but not until the key is released)

* Holding f does not type an f, but instead turns h/j/k/l into arrow keys and
u/i into left/right parentheses for as long as you hold f

* All other keys behave normally


## Configuration Reference

`sdiol` is configured in Lua.  A config is required, and can either be at the
default location (`/etc/sdiol/conf.lua`) or at a location given by the
`--config` option.

The Lua functions available for configuration are as follows:


### `grab_keyboard(REGEX, MAP)`

Grab any keyboard with a name matching REGEX and assign it a keymap of MAP.
REGEX should be a string and MAP should be a Lua table with string keys.  The
keys of the table should be the names of Linux inputs, as you might find either
through the use of the `--verbose` flag or by looking them up in
`/usr/include/linux/input-event-codes.h`.  The values in MAP indicate what
action should be taken when the corresponding key is pressed.

There are four types of actions:

* a key name, such as `KEY_A`

* a `dual_key()`, to indicate a key which can one of two actions based on
whether it is tapped or held

* a `macro()`, `shift()` or `ctrl()`, to indicate a sequence of keys

* another keymap like MAP, to indicate that a key exposes an
alternate keymapping on the keyboard


### `ignore_keyboard(REGEX)`

Don't grab any keyboard with a name matching REGEX.  Multiple `grab_keyboard()`
and `ignore_keyboard()` functions can be in the configuration, and the first
REGEX to match a keyboard name determines the behavior for that keyboard.


### `dual_key(TAP, HOLD [, MODE])`

A key which takes one of two actions based on how long it is held.  Both TAP
and HOLD should be actions.  Note that TAP cannot be a keymap action, and
neither TAP nor HOLD can be nested `dual_key()` actions.

A key is "held" if it is pressed for more than 200ms (not yet configurable) or
if some second key is both pressed and released before the first key is pressed
(press A, press B, release B, release A).  The in-between case (press A, press
B, release A, release B) is called "rollover", and its behavior is configurable
using the optional MODE argument.  MODE can be one of:

* `TAP_ON_ROLLOVER` (the default) should be used for making dual-mode keys out
of normal keys, like f.  When you press f, press another key, release f, and
release the other key, (a "rollover" situation in typing) you will get the
behavior specified by the TAP argument (probably a normal f)

* `HOLD_ON_ROLLOVER` should generally be used for making dual-mode keys out of
modifier keys, like shift.  When you press shift, press another key, release
shift, and release the other key, you will get the behavior specified by the
HOLD argument (probably apply shift to the second key).


### `macro(...)`, `shift(...)`, `ctrl(...)`

A macro is series of key names which should be pressed/released in sequence.
`shift()` and `ctrl()` are just macros where a modifier key will be held
throughout.  Key macros are composable, meaning that the key action defined by:

    macro(KEY_A, KEY_B, KEY_C, shift(KEY_A, KEY_B, KEY_C))

would type `abcABC` when triggered.


## Serving Over A Network

A note on terminology: Here, the "server" refers to the machine with a keyboard
and/or mouse plugged in to it, and the "client" refers to the machine on which
you want to receive keyboard and/or mouse events.

Using `sdiol serve` effectively has some additional requirements:

* Have working `ssh` access from the client to the server.

* Install the openbsd variant of netcat installed on the server (or an
appropriate substitute).

* A working knowledge of file permissions so other users with access to the
server are not able to log all your keystrokes.


### Configuring the `sdiol` server

A simple `/etc/sdiol/conf.lua` for grabbing a keyboard and mouse might look
like this:

    grab_keyboard(".*keyboard.*", {})

    -- future mouse support will include mouse-specific features,
    -- but until then mice are grabbed with grab_keyboard()
    grab_keyboard(".*mouse.*", {})

Then, assuming your user name was `bob` and you wanted to make sure that only
your user could access the `sdiol` server, you could start it like this:

    sudo sdiol serve /run/sdiol.sock --chown-socket bob:bob

Be sure to check out the `--chown-socket` and `--chmod-socket` options and make
sure you get the correct permissions on your socket for your environment.


### Connecting the `sdiol` client

`sdiol` does not do secure networking at all, so instead we will use `ssh` to
encrypt the connection to the server, then on the server we will use `nc` to
connect to the socket.  This leaves `sdiol` to simply read events from stdin:

    ssh bob@myserver nc -U /tmp/sdiol.sock | sudo sdiol read

And you're in business!

If a new client connects to the `sdiol` server, the first client will be
disconnected.  In the future, `sdiol` will be able to maintain multiple client
connections and will support cycling through them using a keybinding on the
shared keyboard.


## Building

First install dependencies:

* Archlinux: `sudo pacman -S libsystemd liblua cmake`

* Debian/Ubuntu: `sudo apt install libsystemd-dev liblua5.3-dev cmake`

Then run the following build steps from the `sdiol` directory:

    mkdir build
    cd build
    cmake ..
    make


## Installing

From the build directory, just run:

    sudo make install

When you are satisfied with your `sdiol` configuration at `/etc/sdiol/conf.lua`,
you can start `sdiol` as a background service:

    sudo systemctl start sdiol.service

    # Then make sure it's working:
    sudo systemctl status sdiol.service

And if you want `sdiol` to start automatically on boot:

    sudo systemctl enable sdiol.service


## Acknowledgements

`sdiol` began as a fork of [dzhu/keytap](https://github.com/dzhu/keytap).  Many
thanks to [dzhu](https://github.com/dzhu) for innovating such a flexible
architecture.

`khash.h` was taken from
[attractivechaos/klib](https://github.com/attractivechaos/klib).  Many thanks
to [attractivechaos](https://github.com/attractivechaos) for maintaining a
freely-available generics library for C.


## License

The file `khash.h` is from attractivechaos's
[klib](https://github.com/attractivechaos/klib) project.

All other source files are in the public domain, under the conditions of the
[Unlicense](https://unlicense.org/).
