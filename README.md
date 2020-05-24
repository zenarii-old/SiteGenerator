# Static Site Generator

----
## Description
A static site generator, written in C, using a custom format. Designed to simplify the creation and editing of html files. An example of the ouput can be seen in [my site](https://zenarii.co.uk) or in the sites folder in the repository.

----
## Get Started
1. Compile generator.c, the exe doesn't need to be linked with. The build.bat uses clang.
2. Create your own `.zsl` files using the custom format.
3. Create your own `style.css`, or use the example in the sites directory.


----
## Command line arguments:

- `--html` Tells the generator to convert the input into html.
- `--author <name>` The author for the `<meta>` html tag.
- `--header <path>.html` The path to a html file to be included at the top of each file.
- `--footer <path>.html` The path to a html file to be included at the bottom of each file.
- `--title <title>` The title for all files

All other arguments are assumed to be input files.

----
## Language Features
- `@Title {<text>}` outputs a h1 with the inside text.
- `@SubTitle {<text>}` outputs a h2 with the inside text.
- `@Image {<alt text> | <source path>}` Outputs an image.
- `@Link {<text> | <link>}` Outputs a link that can be inline with text.
- `@Column[<Type1><Type2>] {<field 1> | <field2> | Optional: <field 3> <field 4>}` Outputs 2 side by side columns that collapse when the screen gets too small. Allowed types are:
  - `I` An image. Uses 1 field as path to the image.
  - `T` Some Text. Uses 1 field as the text stored.
  - `L` An image that is also a hyper link. Uses 2 fields, hyper linked location and image source.
  - `TT` A column with both a title and some text. Uses 2 fields, the title text and the standard text.

All these arguments take plain text, or a string delimited by `"`. Strings are useful when you want to include characters that are reserved by the parser, such as `@` and `|`. Pipe (`|`) is used as a seperator for arguments.
