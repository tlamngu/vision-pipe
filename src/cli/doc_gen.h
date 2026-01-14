/**
 * VisionPipe Documentation Generator
 * 
 * Generates HTML, Markdown, and JSON documentation for VisionPipe interpreter items.
 */

#ifndef VISIONPIPE_DOC_GEN_H
#define VISIONPIPE_DOC_GEN_H

#include <string>
#include <map>

namespace visionpipe {

struct DocGenOptions {
    std::string outputDir = ".";
    std::string outputFormat = "html";  // html, md, json
};

/**
 * Generate documentation for all registered interpreter items.
 * @param opts Documentation generation options
 * @return 0 on success, non-zero on failure
 */
int generateDocs(const DocGenOptions& opts);

}  // namespace visionpipe

#endif  // VISIONPIPE_DOC_GEN_H
