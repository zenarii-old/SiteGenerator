# Static Site Generator

----
## Description
A static site generator, written in C, using a custom format. Designed to simplify the creation and editing of html files. An example of the ouput can be seen in [my site](https://zenarii.co.uk) or in the sites folder in the repository.

----
## Get Started
1. Compile generator.c, the exe doesn't need to be linked with. The build.bat uses clang.
2. Create your own `.zsl` files using the custom format.
3. Create your own css, or use the example in the sites directory.


----
## Command line arguments:

- `--html` Tells the generator to convert the input into html.
- `--author <name>` The author for the `<meta>` html tag.
- `--header <path>.html` The path to a html file to be included at the top of each file.
- `--footer <path>.html` The path to a html file to be included at the bottom of each file.
- `--title <title>` The title for all files
----
## Language Features
- `@Title {<text>}` outputs a h1 with the inside text.
- `@SubTitle {<text>}` outputs a h2 with the inside text.
- `@Image {<alt text>, <source path>}` Outputs an image.
- `@Link {<text>, <link>}` Outputs a link that can be inline with text.

Plain text is used as both the arguments to these identifiers and is also outputted a html `<p>` tag. Using quotation marks around some text allows all interior symbols to be ignored and considered as text.
