Plus42 skin description (*.layout) file format:
Anything from a '#' until the end of the line is a comment
Non-comment lines contain the following information:

(Note: the skin bitmap is assumed to have the same filename as the skin
description, with the 'layout' extension replaced by 'gif'.)
(Note: rectangles are given as "x,y,width,height"; points are "x,y".)

Skin: the portion of the skin bitmap to be rendered as the actual faceplate
Display: describes the location, size, and color of the display; arguments
  are: top-left corner, x magnification, y magnification, background color,
  foreground color. Colors are specified as 6-digit hex numbers in RRGGBB
  format.
Key: describes a clickable key; arguments are: keycode, sensitive rectangle
  (i.e. the rectangle where mouse-down events will cause the key to be
  pressed), display rectangle (i.e. the rectangle that changes when a key is
  pressed or released), and the location of the top-left corner of the active-
  state bitmap (since the active-state bitmap must have the same size as the
  display rectangle, only its position, not its width and height, are
  specified).
  Keycodes in the range 1..37 correspond to actual calculator keys; keycodes
  38..255 can be used to define "macro" keys. For each such keycode, there must
  be a corresponding "Macro:" line in the layout file.
  You may specify two keycodes (two numbers separated by a comma); if you do,
  the first is used when the calculator's shift (indicated by the shift
  annunciator) is inactive, and the second is used when the calculator's shift
  is active. This feature allows you to have a key's shifted function be
  something different than it is on the original HP-42S keyboard.
Macro: for keys with keycodes in the range 38..255, this defines the sequence
  of HP-42S keys (keycodes 1..37) that is to be pressed; arguments are:
  keycode, followed by zero or more keycodes in the range 1..37. See below for
  an example.
Annunciator: describes an HP-42S annunciator; arguments are: code (1=updown,
  2=shift, 3=print, 4=run, 5=battery, 6=g, 7=rad), display rectangle, and the
  location of the top-left corner of the active-state bitmap.

Plus42 uses the same skins as Free42, but in order to fit in the larger
display, it has to stretch the skin images. For most skins, it can handle this
automatically, but for some, it needs some help. In order to provide that help,
I have defined a couple of hints to allow Plus42 to adapt existing skins
better.

The display hints can be added to a skin's layout file. There are two:

DisplayExpansionZone: first_y last_y
  This specifies the strip of the skin image that should be replicated when
  stretching the skin. The strip spans the entire width of the skin, and runs
  from first_y at the top to last_y at the bottom.
  If unspecified, Plus42 uses the top and bottom of the default display
  rectangle. Use this hint if the default strip contains a logo or other
  graphical elements that should not be replicated.
DisplaySize: cols,rows disp_y pixel_scale_y max_rows
  If the skin was designed for a display size other than 2 rows by 22 columns,
  use this hint to specify the default size. The number or rows must be at
  least 2 and the number of columns must be at least 22.
  Disp_y and pixel_scale_y specify top of the display and the the vertical
  pixel size to be used, instead of the ones specified in the Display: line, if
  the number of rows is set to something greater than 2; and max_rows is the
  maximum number of rows that may be used with this skin.
  The disp_y, pixel_scale_y, and max_rows parameters are optional. They may be
  omitted, or if you don't want to specify one but do want to specify one of
  the later ones, use -1 to indicate you don't want to specify a vaiue.

In addition, Plus42 can be configured to take advantage of expanded keyboards,
by rearranging a couple of items, using the Flags: line.

Flags: The "flags" line is a combination of these options: 1 = put FCN on the
  first row of CATALOG and DIRS on the second, instead of the other way around;
  2 = put MEM on the first row of CATALOG and UNITS on the second, instead of
  the other way around; and 4 = put TVM on Shift-0 instead of TOP.FCN. When
  there is no "flags," that's the same as Flags: 0.

Finally, Plus42 can change the X<>Y, E, period, and R/S keys, to (, ), period/
comma, and =, respectively, while the equation editor is active. To specify the
alternate sections of skin background, and the alternate pressed-key images,
use the AltBkgd and AltKey elements:

AltBkgd: mode src_rect dst_point
  Defines an image to be overlaid on the skin when the given mode is in effect.
  Note that there can be multiple AltBkgd elements for each mode; all of them
  will be drawn when the given mode is in effect.
AltKey: mode code src_point
  Defines an alternate pressed-key image to be used instead of the default
  pressed-key, when the given mode is in effect, and for the given key code.
  Note that only the image location is specified, not its size. The size is
  assumed to match the size of the default pressed-key image for the given key
  code.

The mode can be 1 (equation editor in RDX. mode) or 2 (equation editor in RDX,
mode).


For examples, look at the *.layout and *.gif files in this directory.

Macro example:
To define a key for the FIX command, using key code 38: the sequence of
calculator keys for FIX is Shift (28), E (16), Σ+ (1), so...

Key: 38 <sens_rect> <disp_rect> <active_pt>
Macro: 38 28 16 1

You can also define PC keyboard mappings in the *.layout file. The syntax is
identical to that of the keymap file, preceded by a tag that indicates the
target platform: WinKey for Windows, MacKey for Mac, and GtkKey for Linux and
other Unix-like environments. It is necessary to specify which platform each
key mapping is for, since the key codes are platform-dependent.
If a layout file defines a mapping for a key that is also mapped in the keymap
file, the skin-specific mapping takes precedence.
Note that, while Macro definitions may only contain codes 1..37, a keyboard
mapping may contain codes 38..255 as well, so you could theoretically map a PC
keyboard key to a sequence of macros. This is not recommended, however; for
clarity, it is probably better for key mappings to consist only of one key or
macro number, preceded by Shift (28) if necessary. This will also allow Plus42
to match the PC keyboard key to a skin-defined key, which will be highlighted
for visual feedback when the mapping is activated.
