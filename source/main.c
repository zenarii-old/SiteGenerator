//TODO(Zen): way to generate a style sheet
//TODO(Zen): Better error handling
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define log(...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#define INLINE 1

/*
FILE IO
*/

static char *
LoadEntireFile(char * Path) {
    FILE * File = fopen(Path, "r");
    if(!File) 
        return 0;
    
    fseek(File, 0L, SEEK_END);
    long BytesRead = ftell(File);
    
    fseek(File, 0, SEEK_SET);
    
    char * Buffer = calloc(BytesRead + 1, sizeof(char));
    
    if(!Buffer) 
        return 0;
    
    fread(Buffer, sizeof(char), BytesRead, File);
    
    fclose(File);
    return Buffer;
}

/*
Char functions
*/

static int
CharIsAlpha(char c) {
    return (('a' <= c) && (c <= 'z')) || (('A' <= c) && (c <= 'Z'));
}

static int
CharIsDigit(char c) {
    return ('0' <= c) && (c <= '9');
}

static int
CharIsImportantSymbol(char c) {
    return (c == '{') || (c == '}') || (c == '|') || (c == '@');
}


static int
CharIsTextSymbol(char c) {
    return (c == ',') || (c == '#') || (c == '.') || (c == '/');
}

static int
StringCompareCaseInsensitiveN(char * a, char * b, int n) {
    int Matched = 0;
    
    if(a && b && n) {
        Matched = 1;
        for(int i = 0; i < n; ++i) {
            char _a = ('a' <= a[i] && a[i] <= 'z') ? a[i] + ('A' - 'a') : a[i];
            char _b = ('a' <= b[i] && b[i] <= 'z') ? b[i] + ('A' - 'a') : b[i];
            
            if(_a != _b) {
                Matched = 0;
                break;
            }
        }
    }
    return Matched;
}

/*
Identifiers
*/

typedef enum identifier_type identifier_type;
enum identifier_type {
    IDENTIFIER_Invalid,
#define TAG(name, argc, in_line) IDENTIFIER_##name,
#include "tags.inc"
    
    IDENTIFIER_COUNT
};

typedef enum page_node_type page_node_type;
enum page_node_type {
    PAGE_NODE_Invalid,
    
    PAGE_NODE_Identifier,
    PAGE_NODE_Text,
    PAGE_NODE_Break,
    
    PAGE_NODE_COUNT
};

typedef struct page_node page_node;
struct page_node {
    page_node_type pnType;
    page_node * Next;
    page_node * Child;
    
    struct identifier {
        identifier_type iType;
    } Identifier;
    
    struct text {
        char * String;
        int StringLength;
    } Text;
};


/*
TODO(Zen): Memory arena stuff
*/

static page_node TEMPArena[1000];
static int TEMPCursor;

static page_node *
AllocateNode() {
    return TEMPArena + TEMPCursor++;
}

/*
Tokens
*/

typedef enum token_type token_type;
enum token_type {
    TOKEN_Invalid,
    TOKEN_Identifier,
    TOKEN_Text,
    TOKEN_Symbol,
    TOKEN_DoubleNewline,
    //TOKEN_String, (does the same as text but need to strip the quote marks
    TOKEN_COUNT
};

typedef struct token token;
struct token {
    token_type tType;
    char * String;
    int StringLength;
};

typedef struct tokeniser tokeniser;
struct tokeniser {
    char * At;
    int Line;
};

static token
GetNextTokenFromCharacterBuffer(char * Buffer) {
    token Token = {0};
    int i = 0;
    for(; Buffer[i]; ++i) {
        if(CharIsImportantSymbol(Buffer[i])) {
            Token.tType = TOKEN_Symbol;
            Token.String = Buffer + i;
            Token.StringLength = 1;
            break;
        }
        else if(CharIsDigit(Buffer[i])||CharIsAlpha(Buffer[i])||CharIsTextSymbol(Buffer[i])) {
            Token.tType = TOKEN_Text;
            Token.String = Buffer + i;
            //NOTE(Zen): This may be different on windows bc of \r\n
            while(Buffer[i] && !CharIsImportantSymbol(Buffer[i]) && !(Buffer[i] == '\n' && Buffer[i+1] == '\n')) {
                ++i;
            }
            Token.StringLength = (Buffer + i) - Token.String;
            break;
        }
        else if(Buffer[i] == '\n' && Buffer[i+1] == '\n') {
            Token.tType = TOKEN_DoubleNewline;
            Token.String = Buffer + i;
            Token.StringLength = 2;
            break;
        }
    }
    
    //if(Buffer[i] == '\0') log("End of file");
    
    return Token;
}

static token
PeekToken(tokeniser * Tokeniser) {
    return GetNextTokenFromCharacterBuffer(Tokeniser->At);
}

static token
NextToken(tokeniser * Tokeniser) {
    token Token = GetNextTokenFromCharacterBuffer(Tokeniser->At);
    Tokeniser->At = Token.String + Token.StringLength;
    return Token;
}

static int
MatchToken(tokeniser * Tokeniser, char * String, token * Out) {
    int Matched = 0;
    token Token = GetNextTokenFromCharacterBuffer(Tokeniser->At);
    if(StringCompareCaseInsensitiveN(Token.String, String, Token.StringLength)) {
        Matched = 1;
        
        if(Out) {
            *Out = Token;
        }
    }
    
    return Matched;
}

static int
RequireToken(tokeniser * Tokeniser, char * String, token * Out) {
    int Matched = 0;
    token Token = GetNextTokenFromCharacterBuffer(Tokeniser->At);
    if(StringCompareCaseInsensitiveN(Token.String, String, Token.StringLength)) {
        Matched = 1;
        
        Tokeniser->At = Token.String + Token.StringLength;
        if(Out) {
            *Out = Token;
        }
    }
    
    return Matched;
}

/*
Parsing
*/
static void
PrintPageNode(page_node * Node, int Depth) {
    for(int i = 0; i < Depth; ++i)
        fprintf(stderr, "  ");
    
    switch(Node->pnType) {
        case PAGE_NODE_Invalid: {
            fprintf(stderr, "Node (Invalid)\n");
        } break;
        case PAGE_NODE_Identifier: {
            fprintf(stderr, "Node (Identifier, %d)\n", Node->Identifier.iType);
            PrintPageNode(Node->Child, Depth + 1);
        } break;
        case PAGE_NODE_Text: {
            fprintf(stderr, "Node (Text, %.*s)\n", Node->Text.StringLength, Node->Text.String);
        } break;
        case PAGE_NODE_Break: {
            fprintf(stderr, "Node (Newline)\n");
        } break;
        default: {
            fprintf(stderr, "@Zenarii add page node type (%d) to printpagenode\n", Node->pnType);
        }
    }
    
    if(Node->Next) PrintPageNode(Node->Next, Depth);
}

static page_node * ParseFile(tokeniser * Tokeniser);

static page_node *
ParseIdentifier(tokeniser * Tokeniser) {
    //TODO(Zne);
    token Name = GetNextTokenFromCharacterBuffer(Tokeniser->At);//NextToken(Tokeniser);
    Tokeniser->At = Name.String + Name.StringLength;
    RequireToken(Tokeniser, "{", 0);
    
    page_node * Identifier = 0;
    
    if(0) { }
    //TODO(Zen): May change this to total the arguments found.
#define TAG(name, argc, in_line) \
else if (StringCompareCaseInsensitiveN(#name, Name.String, strlen(#name))) { \
/*TODO(Zen): Error checking*/\
Identifier = AllocateNode(); \
Identifier->pnType = PAGE_NODE_Identifier; \
Identifier->Identifier.iType = IDENTIFIER_##name; \
page_node ** ArgStore = &Identifier->Child; \
for(int i = 0; i < argc; ++i) { \
*ArgStore = ParseFile(Tokeniser); \
if(i + 1 < argc) RequireToken(Tokeniser, "|", 0); \
ArgStore = &((*ArgStore)->Next); \
}\
}
#include "tags.inc"
    else {
        //TODO(Zen): Better error reporting
        log("Error: Unknown Identifier %.*s", Name.StringLength, Name.String);
        //TODO(Zen): continue until reaching the next } character
    }
    
    if(!RequireToken(Tokeniser, "}", 0)) {
        //TODO(Zen): Better error handling
        log("Expected an } ");
    }
    
    return Identifier;
}


//TODO(Zen): A proper tokeniser that allows errors
static page_node * 
ParseFile(tokeniser * Tokeniser) {
    page_node * Root = AllocateNode();
    page_node ** NodeStore = &Root;
    
    token Token = {0};
    while(PeekToken(Tokeniser).tType != TOKEN_Invalid) {
        if(RequireToken(Tokeniser, "@", 0)) {
            *NodeStore = ParseIdentifier(Tokeniser);
        }
        else if(PeekToken(Tokeniser).tType == TOKEN_DoubleNewline) {
            NextToken(Tokeniser);
            page_node * Newline = AllocateNode();
            Newline->pnType = PAGE_NODE_Break;
            *NodeStore = Newline;
        }
        else if(PeekToken(Tokeniser).tType == TOKEN_Text) {
            Token = NextToken(Tokeniser);
            page_node * Text = AllocateNode();
            Text->pnType = PAGE_NODE_Text;
            Text->Text.String = Token.String;
            Text->Text.StringLength = Token.StringLength;
            *NodeStore = Text;
        }
        else if(MatchToken(Tokeniser, "|", 0) || MatchToken(Tokeniser, "}", 0)) {
            break;
        }
        
        NodeStore = &(*NodeStore)->Next;
    }
    
    return Root;
}

/*
HTML output
*/
#define foreach(x) for(; (x); (x) = (x)->Next)
static int InParagraph;
static void OutputNodeAsHTML(FILE *, page_node *);

//NOTE(Zen): Add custom identifiers here

static void
OutputSubtitleAsHTML(FILE * File, page_node * Subtitle) {
    char * Text = Subtitle->Child->Text.String;
    int TextLength = Subtitle->Child->Text.StringLength;
    
    fprintf(File, "<h2>%.*s</h2>\n", TextLength, Text);
}

static void
OutputTitleAsHTML(FILE * File, page_node * Title) {
    char * Text = Title->Child->Text.String;
    int TextLength = Title->Child->Text.StringLength;
    
    fprintf(File, "<h1>%.*s</h1>\n", TextLength, Text);
}

static void
OutputLinkAsHTML(FILE * File, page_node * Link) {
    //TODO(Zen): Potentially move this into the macro?
    char * Address = Link->Child->Text.String;
    int AddressLength = Link->Child->Text.StringLength;
    Link->Child = Link->Child->Next;
    
    fprintf(File, "<a class='Link' href='%.*s'>", AddressLength, Address);
    OutputNodeAsHTML(File, Link->Child);
    fprintf(File, "</a>");
}

static void
OutputEmbedAsHTML(FILE * File, page_node * Embed) {
    //Assert(Embed->Child->pnType == PAGE_NODE_Text);
    char * Path = Embed->Child->Text.String;
    int PathLength = Embed->Child->Text.StringLength;
    if(Path[PathLength-1] == ' ') PathLength -= 1;
    
    char * PathStr = strndup(Path, PathLength);
    
    char * Contents = LoadEntireFile(PathStr);
    
    fprintf(File, Contents);
    
    free(PathStr);
    free(Contents);
}

static void
OutputColourAsHTML(FILE * File, page_node * Colour) {
    fprintf(File, "<span style='color:%.*s'>", Colour->Child->Text.StringLength, Colour->Child->Text.String);
    page_node * Contents = Colour->Child->Next;
    OutputNodeAsHTML(File, Contents);
    fprintf(File, "</span>");
}

static void
OutputImageAsHTML(FILE * File, page_node * Image) {
    char * Source = Image->Child->Text.String;
    int SourceLength = Image->Child->Text.StringLength;
    
    fprintf(File, "<div class='ImageContainer'>\n");
    fprintf(File, "\t<img src='%.*s' alt='Image'>\n", SourceLength, Source);
    fprintf(File, "</div>\n");
}


static void
OutputNodeAsHTML(FILE * File, page_node * Node) {
    switch(Node->pnType) {
        case PAGE_NODE_Text: {
            fprintf(File, "%.*s", Node->Text.StringLength, Node->Text.String);
        } break;
        case PAGE_NODE_Break: {
            if(InParagraph) {
                InParagraph = 0;
                fprintf(File, "</p>\n");
            }
            fprintf(File, "<br>\n");
        } break;
        case PAGE_NODE_Identifier: {
            switch(Node->Identifier.iType) {
#define TAG(name, argc, in_line) \
case IDENTIFIER_##name: { \
if(InParagraph && !in_line) { \
fprintf(File, "</p>\n"); \
InParagraph = 0; \
}\
Output##name##AsHTML(File, Node); \
                \
} break;
#include "tags.inc"
                default: break;
            }
        } break;
        case PAGE_NODE_Invalid: {
            log("Invalid Page node");
        } break;
        case PAGE_NODE_COUNT: break;
    }
}

typedef struct site_info site_info;
struct site_info {
    char * Author;
    char * PageTitle;
    char * FileName; //?
};

static void
OutputPageAsHTML(char * Path, site_info SiteInfo, char * Header, char * Footer, page_node * Node) {
    FILE * File = fopen(Path, "w");
    
    fprintf(File, "<!DOCTYPE html>\n");
    fprintf(File, "<html lang='en'>\n");
    
    fprintf(File, "<head>\n");
    {
        fprintf(File, "<!-- Auto generated by Zenarii's Static Website Generator: https://github.com/Zenarii -->\n");
        fprintf(File, "<link rel='stylesheet' type='text/css' href='style.css'>\n");
        fprintf(File, "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n");
        fprintf(File, "<meta charset='UTF-8'>\n");
        if(SiteInfo.Author) fprintf(File, "<meta name='author' content='%s'>\n", SiteInfo.Author);
        if(SiteInfo.PageTitle) fprintf(File, "<title>%s</title>\n", SiteInfo.PageTitle);
    }
    fprintf(File, "</head>\n");
    
    fprintf(File, "<body>\n");
    {
        if(Header) fprintf(File, "%s\n", Header);
        
        foreach(Node) {
            if(Node->pnType == PAGE_NODE_Text && !InParagraph) {
                InParagraph = 1;
                fprintf(File, "<p>");
            }
            OutputNodeAsHTML(File, Node);
        }
        if(InParagraph) fprintf(File, "</p>");
        if(Footer) fprintf(File, "%s\n", Footer);
        
        //TODO(Zen): Push the footer
        fprintf(File, "\n");
    }
    fprintf(File, "</body>\n");
}

/*
Main
*/
#define MAX_FILES_TO_PARSE 64

int main(int argc, char ** argv) {
    site_info SiteInfo = {0};
    char * HeaderPath = 0;
    char * Header = 0;
    char * FooterPath = 0;
    char * Footer = 0;
    
    for(int i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "--header") == 0) {
            HeaderPath = argv[i + 1];
            argv[i] = 0;
            argv[i+1] = 0;
            ++i;
        }
        else if(strcmp(argv[i], "--footer") == 0) {
            FooterPath = argv[i + 1];
            argv[i] = 0;
            argv[i + 1] = 0;
            ++i;
        }
        else if(strcmp(argv[i], "--author") == 0) {
            SiteInfo.Author = argv[i + 1];
            argv[i] = 0;
            argv[i + 1] = 0;
            ++i;
        }
        else if(strcmp(argv[i], "--title") == 0) {
            SiteInfo.PageTitle = argv[i + 1];
            argv[i] = 0;
            argv[i + 1] = 0;
            ++i;
        }
    }
    
    if(HeaderPath) Header = LoadEntireFile(HeaderPath);
    if(FooterPath) Footer = LoadEntireFile(FooterPath);
    
    for(int i = 1; i < argc; ++i) {
        if(argv[i]) {
            char * File = LoadEntireFile(argv[i]);
            unsigned int FileNameLen = 0;
            for(; FileNameLen < strlen(argv[i]); FileNameLen++) {
                if(argv[i][FileNameLen] == '.') break;
            }
            
            char Path[256] = {0};
            strncpy(Path, argv[i], FileNameLen);
            strcat(Path, ".html");
            
            tokeniser Tokeniser = {0};
            Tokeniser.At = File;
            page_node * Node = ParseFile(&Tokeniser);
            //log("--------");
            //PrintPageNode(Node, 0);
            OutputPageAsHTML(Path, SiteInfo, Header, Footer, Node);
        }
    }
    log("Done");
    
    return 0;
}