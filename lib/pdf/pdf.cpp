#include "pdf.h"
#include <iostream>
#include <podofo/podofo.h>
using namespace PoDoFo;
using namespace std;

int pdf_merge(const char* file1, const char* file2, const char* output)
{
    try {
        PdfMemDocument pdf1;
        pdf1.Load(file1);
        PdfMemDocument pdf2;
        pdf2.Load(file2);
        pdf1.GetPages().AppendDocumentPages(pdf2);
        pdf1.Save(output);
    } catch (const PdfError &e) {
        std::cerr << "Ошибка PoDoFo: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int pdf_add_watermark(const char* file, const char* output)
{
    try
    {

        PdfMemDocument document;
        document.Load(file);

        auto image = document.CreateImage();
        PdfImageInfo imageInfo = image->Load("media/logo.png");

        // Scale factor: 1/4 (0.25)
        double scale = 0.25;
        double scaledWidth = imageInfo.Width * scale;
        double scaledHeight = imageInfo.Height * scale;

        // Add watermark to each page
        for (size_t i = 0; i < document.GetPages().GetCount(); ++i)
        {
            auto& page = document.GetPages().GetPageAt(i);
            Rect pageRect = page.GetRect();

            // Position: bottom-right corner
            // In PDF, origin is at bottom-left, so we need to position it at:
            // x = right_edge - scaled_width - margin
            // y = bottom_edge + margin (since it's already at bottom)
            double margin = 10.0;  // 10 points margin from edges
            double x = pageRect.GetRight() - scaledWidth - margin;
            double y = pageRect.GetBottom() + margin;

            PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawImage(*image, x, y, scale, scale);
            painter.FinishDrawing();
        }

        document.Save(output);

        cout << "Success! Watermark added to all pages." << endl;
        return 0;
    }
    catch (PdfError& err)
    {
        cerr << "PdfError: " << err.what() << endl;
        err.PrintErrorMsg();
        return (int)err.GetCode();
    }
    catch (exception& err)
    {
        cerr << "Error: " << err.what() << endl;
        return 1;
    }
}
