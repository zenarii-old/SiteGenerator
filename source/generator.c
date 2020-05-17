#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static int DidError = 0;

static char *
LoadEntireFileNT(char * FileName) {
    char * Result = 0;
    FILE * File = fopen(FileName, "rb");
    if(File) {
        fseek(File, 0, SEEK_END);
        int FileSize = ftell(File);
        fseek(File, 0, SEEK_SET);
        Result = malloc(FileSize + 1);
        if(Result) {
            fread(Result, 1, FileSize, File);
            Result[FileSize] = 0;
        }
    }
    else {
        printf("Failed to read file %s.\n", FileName);
    }

    return Result;
}

/*
    String/Character functions
*/
static int
CharIsAlpha(char c) {
    return (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'));
}

static int
CharIsDigit(char c) {
    return ('0' <= c && c <= '9');
}

static int
CharIsSymbol(char c) {
    return c == '.' || c == ':' || c == '/' || c == '\\';
}

static int
StringCompareCaseSensitiveR(const char * String1, const char * String2, int Length) {
    int Match = 0;
    if(String1 && String2 && (Length > 0)) {
        Match = 1;
        for(int i = 0; i < Length; ++i) {
            if(String1[i] != String2[i]) return 0;
        }
    }
    return Match;
}


/*
    Debug
*/
#define Assert(expression) InvalidCode(__LINE__, __FILE__, #expression, expression, 1)
#define SoftAssert(expression) InvalidCode(__LINE__, __FILE__, #expression, expression, 0)

static void
InvalidCode(const int Line, const char * File, const char * Error, int Result, int Crash) {
    if(Result) return;

    printf("Assert Failed: %s\nLine: %i File:%s\n", Error, Line, File);

    if(Crash) {
        *((int *)0) = 0;
    }
}

/*
    Dynamic Array/Vector
*/
//Note(Zen): dynamic array/vector implementation
typedef struct vector_header vector_header;
struct vector_header {
    size_t Length;
    size_t Capacity;
    char Buffer[0];
};

#define _VectorHeader(v) ((vector_header *)((char *)v - offsetof(vector_header, Buffer)))
#define _VectorFits(v, n) (VectorLength(v) + (n) <= VectorCapacity(v))
#define _VectorFit(v, n) (_VectorFits(v, n) ? 0 : ((v) = _VectorGrow((v), VectorLength(v) + (n), sizeof(*(v)))))

#define VectorLength(v) ((v) ? _VectorHeader(v)->Length : 0)
#define VectorCapacity(v) ((v) ? _VectorHeader(v)->Capacity : 0)
#define VectorPushBack(v, x) (_VectorFit(v, 1), v[VectorLength(v)] = (x), _VectorHeader(v)->Length++)
#define VectorFree(v) ((v) ? free(_VectorHeader(v)) : 0)

static void *
_VectorGrow(const void * Vector, size_t NewLength, size_t ElementSize) {
    size_t NewCapacity = MAX(1 + 2 * VectorCapacity(Vector), NewLength);
    Assert(NewLength <= NewCapacity);
    size_t NewSize = offsetof(vector_header, Buffer) + NewCapacity * ElementSize;
    vector_header *NewVector;
    if (Vector) {
        NewVector = realloc(_VectorHeader(Vector), NewSize);
    }
    else {
        NewVector = malloc(NewSize);
        NewVector->Length = 0;
    }
    NewVector->Capacity = NewCapacity;
    return NewVector->Buffer;
}


/*
    Site Generator
*/

typedef enum token_type token_type;
enum token_type {
    TOKEN_LASTCHAR = 127,
    TOKEN_IDENTIFIER,
    TOKEN_TEXT,
    TOKEN_NEW_LINE
};

static int Line;

typedef struct token token;
struct token {
    token_type Type;
    const char * Start;
    const char * End;
    int Line;
};

static void
PrintToken(token Token) {
    if(Token.Type <= TOKEN_LASTCHAR) {
        printf("TOKEN: %c\n", Token.Type);
    }
    else if(Token.Type == TOKEN_IDENTIFIER) {
        printf("TOKEN: IDENTIFIER (%.*s)\n", (int)(Token.End - Token.Start), Token.Start);
    }
    else if(Token.Type == TOKEN_TEXT) {
        printf("TOKEN: TEXT (%.*s)\n", (int)(Token.End - Token.Start), Token.Start);
    }
    else if(Token.Type == TOKEN_NEW_LINE) {
        printf("TOKEN: NEWLINE\n");
    }
}
//WARNING(Zen): This function points to a static string, meaning that multiple calls
//will overwrite the string each time. Make sure before calling again you don't need the return
//of the previous call.
static char *
PrintableTokenName(token Token) {
    static char TokenName[32];
    if(Token.Type <= TOKEN_LASTCHAR) {
        sprintf(TokenName, "character %c .", Token.Type);
    }
    else if(Token.Type == TOKEN_IDENTIFIER) {
        sprintf(TokenName, "%.*s", (int)(Token.End - Token.Start), Token.Start);
    }
    else if(Token.Type == TOKEN_TEXT) {
        sprintf(TokenName, "text", (int)(Token.End - Token.Start), Token.Start);
    }
    else if(Token.Type == TOKEN_NEW_LINE) {
        sprintf(TokenName, "newline");
    }
    return TokenName;
}

static char *
AddTokenToBuffer(char * Stream, token ** TokenBuffer) {
    token Token = {0};
    Token.Start = Stream;
    if(*Stream == '@') {
        ++Stream;
        while(CharIsAlpha(*Stream)) {
            ++Stream;
        }
        Token.Type = TOKEN_IDENTIFIER;
    }
    else if(CharIsAlpha(*Stream)) {
        while(CharIsAlpha(*Stream) || CharIsDigit(*Stream) || CharIsSymbol(*Stream) || *Stream == ' ') {
            ++Stream;
        }
        Token.Type = TOKEN_TEXT;
    }
    else if(*Stream == ' ' || *Stream == '\r') {
        //Note(Zen): Skip Spaces as whitespace
        ++Stream;
        return Stream;
    }
    else if(*Stream == '\n') {
        Line++;
        if(*(Stream+1) == '\n' || (*(Stream+1) == '\r' && *(Stream+2) == '\n')) {
            ++Stream;
            Token.Type = TOKEN_NEW_LINE;
        }
        else {
            //Note(Zen): Skip single new lines
            ++Stream;
            return Stream;
        }
    }
    else {
        Token.Type = *Stream++;
    }
    Token.End = Stream;
    Token.Line = Line;
    //PrintToken(Token);
    VectorPushBack((*TokenBuffer), Token);
    return Stream;
}

static int
RequireToken(token_type Type, token Token) {
    if(Token.Type == Type) {
        return 1;
    }
    return 0;
}

static int
MatchToken(token_type Type, token Token) {
    if(Token.Type == Type) {
        return 1;
    }
    return 0;
}

typedef enum page_node_type page_node_type;
enum page_node_type {
    PAGE_NODE_INVALID,
    PAGE_NODE_TITLE,
    PAGE_NODE_SUBTITLE,
    PAGE_NODE_LINK,
    PAGE_NODE_TEXT,
    PAGE_NODE_NEW_PARAGRAPH,
    PAGE_NODE_IMAGE
};

typedef struct page_node page_node;
struct page_node {
    page_node_type Type;
    page_node * Child;
    page_node * Next;
    union {
        struct {
            const char * TitleTextStart;
            const char * TitleTextEnd;
        };

        struct {
            const char * LinkTextStart;
            const char * LinkTextEnd;
            const char * LinkURLStart;
            const char * LinkURLEnd;
        };

        struct {
            const char * TextStart;
            const char * TextEnd;
        };
    };
};

static void
PrintPageNode(page_node PageNode) {
    switch (PageNode.Type) {
        case PAGE_NODE_INVALID: {
            printf("PAGE NODE: INVALID\n");
        } break;
        case PAGE_NODE_TITLE: {
            printf("PAGE NODE: TITLE\n\tText: %.*s\n", (int)(PageNode.TitleTextEnd - PageNode.TitleTextStart), PageNode.TitleTextStart);
        } break;
        case PAGE_NODE_SUBTITLE: {
            printf("PAGE NODE: SUBTITLE\n\tText: %.*s\n", (int)(PageNode.TitleTextEnd - PageNode.TitleTextStart), PageNode.TitleTextStart);
        } break;
        case PAGE_NODE_LINK: {
            printf("PAGE NODE: LINK\n\tText: %.*s\n\tURL: %.*s\n", (int)(PageNode.LinkTextEnd-PageNode.LinkTextStart), PageNode.LinkTextStart, (int)(PageNode.LinkURLEnd-PageNode.LinkURLStart), PageNode.LinkURLStart);
        } break;
        case PAGE_NODE_TEXT: {
            printf("PAGE NODE: TEXT\n\tText: %.*s\n",(int)(PageNode.LinkTextEnd-PageNode.LinkTextStart), PageNode.LinkTextStart, (int)(PageNode.LinkURLEnd-PageNode.LinkURLStart), PageNode.LinkURLStart);
        } break;
        case PAGE_NODE_NEW_PARAGRAPH: {
            printf("PAGE NODE: NEW PARAGRAPH\n");
        } break;
        default: {
            printf("PAGE NODE: @Zen add page node: (%d)\n", PageNode.Type);
        } break;
    }
}

static page_node *
BuildPage(token * TokenBuffer) {
    page_node * PageNodeList = 0;

    for(int i = 0; i < VectorLength(TokenBuffer); ++i) {
        token Token = TokenBuffer[i];
        page_node Node = {0};
        switch(Token.Type) {
            case TOKEN_NEW_LINE: {
                Node.Type = PAGE_NODE_NEW_PARAGRAPH;
            } break;
            case TOKEN_IDENTIFIER: {
                int TokenLength = (Token.End - Token.Start);
                if(StringCompareCaseSensitiveR(Token.Start, "@Title", TokenLength)) {
                    Node.Type = PAGE_NODE_TITLE;
                    ++i;
                    if(RequireToken('{', TokenBuffer[i])) {
                        ++i;
                        if(RequireToken(TOKEN_TEXT, TokenBuffer[i])) {
                            Node.TitleTextStart = TokenBuffer[i].Start;
                            Node.TitleTextEnd = TokenBuffer[i].End;
                        }
                        else {
                            printf("Error: Argument 1 of @Title should be text, not %s", PrintableTokenName(TokenBuffer[i]));
                            DidError = 1;
                        }
                    }
                    else {
                        printf("Error: @Title should be followed by a '{'\n");
                        DidError = 1;
                    }

                    ++i;
                    if(!RequireToken('}', TokenBuffer[i])) {
                        printf("@Title must be followed by '}'\n");
                        DidError = 1;
                    }
                }
                else if(StringCompareCaseSensitiveR(Token.Start, "@Link", TokenLength)) {
                    Node.Type = PAGE_NODE_LINK;
                    ++i;
                    if(RequireToken('{', TokenBuffer[i])) {
                        ++i;
                        if(RequireToken(TOKEN_TEXT, TokenBuffer[i])) {
                            Node.LinkTextStart = TokenBuffer[i].Start;
                            Node.LinkTextEnd = TokenBuffer[i].End;
                        }
                        else {
                            printf("Error: Argument 1 of @Link should be a string, not %s\n", PrintableTokenName(TokenBuffer[i]));
                            DidError = 1;
                        }

                        ++i;
                        if(!RequireToken(',', TokenBuffer[i])) {
                            printf("Error: @Link requires two arguments\n");
                            DidError = 1;
                        }

                        ++i;
                        if(RequireToken(TOKEN_TEXT, TokenBuffer[i])) {
                            Node.LinkURLStart = TokenBuffer[i].Start;
                            Node.LinkURLEnd = TokenBuffer[i].End;
                        }
                        else {
                            printf("Error: Argument 2 of @Link should be text, not %s\n", PrintableTokenName(TokenBuffer[i]));
                        }
                    }
                    else {
                        printf("Error: Require '{' after @Link\n");
                        DidError = 1;
                    }
                    ++i;
                    if(!RequireToken('}', TokenBuffer[i])) {
                        printf("Error: @Link must be followed by '}'\n");
                        DidError = 1;
                    }
                }
                else if(StringCompareCaseSensitiveR(Token.Start, "@SubTitle", TokenLength)) {
                    Node.Type = PAGE_NODE_SUBTITLE;
                    ++i;
                    if(RequireToken('{', TokenBuffer[i])) {
                        ++i;
                        if(RequireToken(TOKEN_TEXT, TokenBuffer[i])) {
                            Node.TitleTextStart = TokenBuffer[i].Start;
                            Node.TitleTextEnd = TokenBuffer[i].End;
                        }
                        else {
                            printf("Error: Argument 1 of @SubTitle should be text, not %s", PrintableTokenName(TokenBuffer[i]));
                            DidError = 1;
                        }
                    }
                    else {
                        printf("Error: @SubTitle should be followed by a '{'\n");
                        DidError = 1;
                    }

                    ++i;
                    if(!RequireToken('}', TokenBuffer[i])) {
                        printf("@SubTitle must be followed by '}'\n");
                        DidError = 1;
                    }
                }
                else if(StringCompareCaseSensitiveR(Token.Start, "@Image", TokenLength)) {
                    Node.Type = PAGE_NODE_IMAGE;
                    ++i;
                    if(RequireToken('{', TokenBuffer[i])) {

                    }
                    else {
                        printf("Error: @Image needs a url and alt text.\n");
                    }
                }
                else {
                    printf("Error: Unknown Identifier (%.*s)\n", (int)(Token.End-Token.Start), Token.Start);
                    DidError = 1;
                }
            } break;
            case TOKEN_TEXT: {
                Node.Type = PAGE_NODE_TEXT;
                Node.TextStart = Token.Start;
                Node.TextEnd = Token.End;
            } break;
            default: {
                printf("Error: Couldn't build page node from token %d: %s\n", i, PrintableTokenName(Token));
            } break;
        }

        PrintPageNode(Node);
        VectorPushBack(PageNodeList, Node);
    }
    return PageNodeList;
}

typedef struct site_info site_info;
struct site_info {
    char * AuthorName;
    char * PageTitle;
};

static void
OutputPageNodeListAsHTML(site_info SiteInfo, page_node * PageNodeList, char * Header, char * Footer) {
    //Note(Zen): Output to the file
    //TODO(Zen): make the out the file name that went in
    FILE * OutputFile = fopen("out.html", "w");

    fprintf(OutputFile, "<!DOCTYPE html>\n");
    fprintf(OutputFile, "<!-- Auto generated by Zenarii's Static Website Generator: https://github.com/Zenarii-->\n");
    fprintf(OutputFile, "<html lang=\"en\">\n");
    fprintf(OutputFile, "<head>\n");
    {
        if(SiteInfo.PageTitle) fprintf(OutputFile, "<title>%s</title>\n", SiteInfo.PageTitle);

        fprintf(OutputFile, "<link rel='stylesheet' type='text/css' href='style.css'>\n");
        fprintf(OutputFile, "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
        fprintf(OutputFile, "<meta charset='UTF-8'>\n");
        //TODO(Zen): Open graph stuff e.g. og:title https://ogp.me/
        if(SiteInfo.AuthorName) fprintf(OutputFile, "<meta name=\"author\" content=\"%s\">\n", SiteInfo.AuthorName);

    }
    fprintf(OutputFile, "</head>\n");

    fprintf(OutputFile, "<body>\n");
    {
        if(Header) fprintf(OutputFile, Header);
        int InParagraph = 0;

        for(int i = 0; i < VectorLength(PageNodeList); ++i) {
            page_node CurrentNode = PageNodeList[i];
            page_node PreviousNode;
            if(i != 0) {
                 PreviousNode = PageNodeList[i-1];
            }
            switch (CurrentNode.Type) {
                case PAGE_NODE_TITLE: {
                    int TextLength = (int)(CurrentNode.TitleTextEnd-CurrentNode.TitleTextStart);

                    fprintf(OutputFile, "<h1>%.*s</h1>\n", TextLength, CurrentNode.TitleTextStart);
                } break;
                case PAGE_NODE_NEW_PARAGRAPH: {
                    if(InParagraph) {
                        fprintf(OutputFile, "\n</div>\n");
                        InParagraph = 0;
                    }
                } break;
                case PAGE_NODE_LINK: {
                    int TextLength = CurrentNode.LinkTextEnd - CurrentNode.LinkTextStart;
                    int LinkLength = CurrentNode.LinkURLEnd - CurrentNode.LinkURLStart;
                    fprintf(OutputFile, "<a href='%.*s' class='Link'>%.*s</a> ", LinkLength, CurrentNode.LinkURLStart, TextLength, CurrentNode.LinkTextStart);
                } break;
                case PAGE_NODE_TEXT: {
                    if(!InParagraph) fprintf(OutputFile, "<div class='BodyText'>\n");

                    int TextLength = CurrentNode.TextEnd - CurrentNode.TextStart;
                    fprintf(OutputFile, "%.*s", TextLength, CurrentNode.TextStart);
                    InParagraph = 1;
                } break;
                case PAGE_NODE_SUBTITLE: {
                    int TextLength = (int)(CurrentNode.TitleTextEnd-CurrentNode.TitleTextStart);
                    fprintf(OutputFile, "<h2>%.*s</h2>\n", TextLength, CurrentNode.TitleTextStart);
                } break;
                default: {
                    printf("@Zen add content for page node (%d)\n", CurrentNode.Type);
                }
            }
        }
        if(InParagraph) {
            fprintf(OutputFile, "\n</div>\n");
        }

        if(Footer) fprintf(OutputFile, Footer);
    }
    fprintf(OutputFile, "</body>\n");

    fprintf(OutputFile, "</html>\n");
    fclose(OutputFile);

    printf("Program reached end.\n");
}

#define MAX_FILES_TO_PARSE 4
#define OUTPUT_HTML (1<<0)

int
main(int argc, char ** args) {
    site_info SiteInfo = {0};
    char * HeaderPath = 0;
    char * FooterPath = 0;
    char * Header = 0;
    char * Footer = 0;
    int FileCount = 0;
    char * FilesToParse[MAX_FILES_TO_PARSE] = {0};
    unsigned int Output = 0;
    //Note(Zen): Parse arguments sent to the program.
    for(int i = 1; i < argc; ++i) {
        //Note(Zen): Single args
        if(strcmp(args[i], "--html") == 0) {
            printf("Outputting as html.\n");
            args[i] = 0;
            Output |= OUTPUT_HTML;
        }
        //Note(Zen): Arguments with some extra info
        else if(strcmp(args[i], "--header") == 0) {
            HeaderPath = args[i+1];
            args[i] = 0;
            args[i+1] = 0;
            ++i;
            printf("Header path is %s.\n", HeaderPath);
        }
        else if(strcmp(args[i], "--footer") == 0) {
            FooterPath = args[i+1];
            args[i] = 0;
            args[i+1] = 0;
            ++i;
            printf("Footer path is %s.\n", FooterPath);
        }
        else if(strcmp(args[i], "--author") == 0) {
            SiteInfo.AuthorName = args[i+1];
            args[i] = 0;
            args[i+1] = 0;
            ++i;
            printf("Author Name is %s.\n", SiteInfo.AuthorName);
        }
        else if(strcmp(args[i], "--title") == 0) {
            SiteInfo.PageTitle = args[i+1];
            args[i] = 0;
            args[i+1] = 0;
            ++i;
            printf("Page title is %s.\n", SiteInfo.PageTitle);
        }
    }

    for(int i = 1; i < argc; ++i) {
        if(args[i]) {
            FilesToParse[FileCount++] = args[i];
        }
    }

    if(HeaderPath) {
        Header = LoadEntireFileNT(HeaderPath);
    }
    if(FooterPath) {
        Footer = LoadEntireFileNT(Footer);
    }

    //Note(Zen): Build a token buffer for each file
    for(int i = 0; i < FileCount; ++i) {
        char * File = LoadEntireFileNT(FilesToParse[i]);
        char * Stream = File;
        printf("Parsing %s.\n", FilesToParse[i]);
        token * TokenBuffer = NULL;
        while(*Stream) {
            Stream = AddTokenToBuffer(Stream, &TokenBuffer);
        }
        for(int i = 0; i < VectorLength(TokenBuffer); ++i) {
            printf("%d", i);
            PrintToken(TokenBuffer[i]);
        }
        printf("\nBuilding Page Tree:\n");
        page_node * PageNodeList = BuildPage(TokenBuffer);

        if(!DidError) {
            if(Output & OUTPUT_HTML) {
                OutputPageNodeListAsHTML(SiteInfo, PageNodeList, Header, Footer);
            }
        }
        else {
            printf("Failed to parse file: %s, due to errors.\n", FilesToParse[i]);

        }
        //Note(Zen): Reset Globals
        Line = 0;
        DidError = 0;
    }

    return 0;
}
