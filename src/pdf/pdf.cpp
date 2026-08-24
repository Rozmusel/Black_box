#include "pdf.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>
#include <podofo/podofo.h>
using namespace PoDoFo;
using namespace std;

int pdfMerge(string file1, string file2, string output)
{
    try {
        spdlog::debug("Merging files: {} and {} into {}", file1, file2, output);
        PdfMemDocument pdf1;
        pdf1.Load(file1);
        PdfMemDocument pdf2;
        pdf2.Load(file2);
        pdf1.GetPages().AppendDocumentPages(pdf2);
        pdf1.Save(output);
    } catch (const PdfError &e) {
        spdlog::error("Ошибка PoDoFo: {}", e.what());
        return 1;
    }
    return 0;
}

int pdfAddWatermark(string file, string output)
{
    try
    {
        spdlog::info("Adding watermark to {}", file);
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

        if (!std::filesystem::exists(std::filesystem::u8path(output).parent_path())) {
            std::filesystem::create_directories(std::filesystem::u8path(output).parent_path());
        }
        document.Save(output);

        spdlog::info("Watermark added successfully to {}", file);
        return 0;
    }
    catch (PdfError& err)
    {
        spdlog::error("PdfError: {}", err.what());
        err.PrintErrorMsg();
        return (int)err.GetCode();
    }
    catch (exception& err)
    {
        spdlog::error("Error: {}", err.what());
        return 1;
    }
}
