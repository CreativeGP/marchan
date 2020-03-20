#include <iostream>

#include <stack>
#include <string>

#include <podofo/podofo.h>

using namespace std;
using namespace PoDoFo;

#define DEBUG(x) if (args == 2) { x; }

enum ParserState {
    PS_NONE,
    PS_TEXT_STREAM,

};

struct Text {
    string fontname;
    double fontsize;
    bool underline, bold, italic, strikedout, subsetting;
    string str;
};

struct TextBlock {
    double x, y;
    vector<Text> texts;
};





int main(int args, char **argv)
{
    PdfMemDocument pdf(argv[1]);

    double dGlobalY = 0;
    double lastLineLead = 0;

    vector<TextBlock> textblocks { {} };

    for (int pn = 0; pn < pdf.GetPageCount(); ++pn) {
        PdfPage* page = pdf.GetPage(pn);

        // Iterate over all the PDF commands on that page:

        PdfContentsTokenizer tok(page);
        const char* token = nullptr;
        PdfVariant var;
        EPdfContentsType type;
        ParserState ps = PS_NONE;
        stack<PdfVariant> stack;


        double dCurPosX     = 0.0;
        double dCurPosY     = 0.0;
        double dCurFontSize = 0.0;
        bool   bTextBlock   = false;
        PdfFont* pCurFont   = NULL;
        string sCurFontName   = "";
        double dPageH = page->GetMediaBox().GetHeight();
        double dPageW = page->GetMediaBox().GetWidth();

        while (tok.ReadNext(type, token, var)) {
            string sToken;
            if (token) sToken = string(token);
            switch (type) {
                case PoDoFo::ePdfContentsType_Keyword:
                    // process token: it contains the current command
                    //   pop from var stack as necessary
                    if (sToken == "ET") ps = PS_NONE;

                    if (sToken == "l" || sToken == "m") {
//                        dCurPosX = stack.top().GetReal();
//                        stack.pop();
//                        dCurPosY = dPageH - stack.top().GetReal();
//                        stack.pop();
                    }

                    if (ps == PS_TEXT_STREAM) {
                        if (sToken == "Tf") {
                            dCurFontSize = stack.top().GetReal();
                            stack.pop();
                            PdfName fontName = stack.top().GetName();
                            stack.pop();

                            PdfObject* pFont = page->GetFromResources( PdfName("Font"), fontName );
                            if( !pFont )
                            {
                                PODOFO_RAISE_ERROR_INFO( ePdfError_InvalidHandle, "Cannot create font!" );
                            }

                            PdfFont *pPrevFont = pCurFont;
                            pCurFont = pdf.GetFont( pFont );
                            if( !pCurFont )
                            {
                                fprintf( stderr, "WARNING: Unable to create font for object %i %i R\n",
                                         pFont->Reference().ObjectNumber(),
                                         pFont->Reference().GenerationNumber() );

                                pCurFont = pPrevFont;
                            }

                            sCurFontName = fontName.GetName();
                            if (pCurFont) {
                                textblocks.back().texts.emplace_back(Text {sCurFontName, dCurFontSize, pCurFont->IsUnderlined(), pCurFont->IsBold(), pCurFont->IsItalic(), pCurFont->IsStrikeOut(), pCurFont->IsSubsetting(), ""});
                            } else {
                                textblocks.back().texts.emplace_back(Text {sCurFontName, 1, false, false, false, false, false, ""});
                            }
                        }

                        if (sToken == "Tj") {
                            if (!var.IsString()) continue;
                            string str = stack.top().GetString().GetStringUtf8();
                            stack.pop();
                            textblocks.back().texts.back().str += str;
                            DEBUG(cout << str << endl);
                        }
                        if (sToken == "TJ") {
                            if (var.IsArray()) {
                                PoDoFo::PdfArray& a = stack.top().GetArray();
                                for (size_t i = 0; i < a.GetSize(); ++i) {
                                    if (a[i].IsString()) {
                                        string str = a[i].GetString().GetStringUtf8();
                                        textblocks.back().texts.back().str += str;
                                        DEBUG(cout << str);
                                    } else if (a[i].IsNumber()) {
                                        int64_t offset = a[i].GetNumber();
                                        // 1000分の１の単位
                                        if (offset < -100)
                                            textblocks.back().texts.back().str += " ";
                                    }
                                }
                            }
                            stack.pop();
                        }
                        if (sToken == "TD") {
                            double dy = stack.top().GetReal(); stack.pop();
                            double dx = stack.top().GetReal(); stack.pop();
                            dCurPosX += dx;
                            dCurPosY -= dy;

                            // だいたい１が正常な移動ライン。横移動はまぁあるので、それは大きく許すことにしてみよう.
                            if (abs(dy) > 1.5 /*|| dx > 10*/) {
                                textblocks.emplace_back( TextBlock {dCurPosX, dGlobalY+dCurPosY, {}} );
                                if (pCurFont) {
                                    textblocks.back().texts.emplace_back(Text {sCurFontName, dCurFontSize, pCurFont->IsUnderlined(), pCurFont->IsBold(), pCurFont->IsItalic(), pCurFont->IsStrikeOut(), pCurFont->IsSubsetting(), ""});
                                } else {
                                    textblocks.back().texts.emplace_back(Text {sCurFontName, 1, false, false, false, false, false, ""});
                                }
                                DEBUG(cout << "--NEW BLOCK--" <<  endl);
                            }

                            DEBUG(cout << "TD " << dx << " " << dy << endl);
                        }
                        if (sToken == "Td") {
                            double dy = stack.top().GetReal(); stack.pop();
                            double dx = stack.top().GetReal(); stack.pop();
                            dCurPosX += dx;
                            dCurPosY -= dy;

                            // だいたい１が正常な移動ライン。横移動はまぁあるので、それは大きく許すことにしてみよう.
                            if (abs(dy) > lastLineLead*1.3 /*|| dx > 10*/) {
                                textblocks.emplace_back( TextBlock {dCurPosX, dGlobalY+dCurPosY, {}} );
                                if (pCurFont) {
                                    textblocks.back().texts.emplace_back(Text {sCurFontName, dCurFontSize, pCurFont->IsUnderlined(), pCurFont->IsBold(), pCurFont->IsItalic(), pCurFont->IsStrikeOut(), pCurFont->IsSubsetting(), ""});
                                } else {
                                    textblocks.back().texts.emplace_back(Text {sCurFontName, 1, false, false, false, false, false, ""});
                                }
                                DEBUG(cout << "--NEW BLOCK--" <<  endl);
                            } else {
                                textblocks.back().texts.back().str += " ";
                            }

                            if (dx == 0) {
                                lastLineLead = abs(dy);
                            }

                            DEBUG(cout << "Td " << dx << " " << dy << endl);
                        }
                        if (sToken == "TD" || sToken == "'" || sToken == "\"" || sToken == "T*") {
    //                        cout << var.GetNumber() << endl;
                            textblocks.back().texts.back().str += "";
                        }
    //                    if (sToken == "TD" || sToken == "Tl" || sToken == "TL")
    //                        cout << endl;
                    }

                    if (sToken == "BT") ps = PS_TEXT_STREAM;

                    while (!stack.empty()) stack.pop();
                    break;
                case PoDoFo::ePdfContentsType_Variant:
                    // process var: push it onto a stack
                    stack.push(var);
                    // cout << sToken << endl;
                    break;
                default:
                    // should not happen!
                    break;
            }

        }
        dGlobalY += dCurPosY;
        //dGlobalY += page->GetMediaBox().GetHeight();

    }

    if (args == 2) return 0;

    string compiled = "<html><head></head><body><style>p { width: 600px; }</style>";

//    for (auto textblock : textblocks) {
//        textblock.x = textblock.x < 0 ? 0 : textblock.x;
//        compiled += "<span style='position: absolute; left: "+to_string(textblock.x)+"em; top: "+to_string(textblock.y)+"em;'>";
//        for (auto text : textblock.texts) {
//            compiled += "<span style='";
//            compiled += "font-family: \"" + text.fontname + "\";";
//            compiled += "font-size: " + to_string(text.fontsize) + "em;";
//            if (text.bold)
//                compiled += "font-weight: bold;";
//            if (text.underline)
//                compiled += "text-decoration-line: underline;";
//            if (text.strikedout)
//                compiled += "text-decoration-line: line-through;";
//            if (text.italic)
//                compiled += "font-style: italic;";

//            compiled += "'>";
//            if (text.subsetting) compiled += "<sub>";
//            compiled += text.str;
//            if (text.subsetting) compiled += "</sub>";
//            compiled += "</span>";
//        }
//        compiled += "</span>";
//    };
    for (auto textblock : textblocks) {
        textblock.x = textblock.x < 0 ? 0 : textblock.x;
        compiled += "<p>";
        for (auto text : textblock.texts) {
            compiled += text.str;
        }
        compiled += "</p>";
    };

    compiled += "</body></html>";

    cout << compiled;

    return 0;
}
