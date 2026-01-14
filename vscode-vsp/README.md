# VisionPipe VSP Language Extension

Syntax highlighting and language support for VisionPipe Script (.vsp) files in Visual Studio Code.

## Features

- **Syntax Highlighting** - Full syntax highlighting for VSP language constructs
- **Code Snippets** - Quick templates for common patterns
- **Auto-completion** - Bracket matching and auto-closing
- **Code Folding** - Fold pipeline blocks and control structures

## VSP Language

VisionPipe Script (VSP) is a domain-specific language for building computer vision pipelines.

### Example

```vsp
// Hello World Pipeline
pipeline main
    video_cap(0)
    draw_text("Hello VisionPipe!", 20, 40, 1.0, "green")
    display("Demo")
    wait_key(1)
end

exec_loop main
```

### Keywords

| Keyword | Description |
|---------|-------------|
| `pipeline` | Define a pipeline block |
| `end` | End a block |
| `exec_seq` | Execute pipeline sequentially |
| `exec_multi` | Execute pipelines in parallel |
| `exec_loop` | Execute pipeline in a loop |
| `use` | Load from cache |
| `cache` | Store to cache |
| `global` | Global scope modifier |
| `let` | Variable binding |

### Cache Operator

Use `->` to cache function outputs:

```vsp
undistort(params) -> "rectified"
stereo_sgbm("left", "right") -> global "disparity"
```

### Built-in Functions

**Video I/O**
- `video_cap(source)` - Capture video
- `video_write(path)` - Write video
- `save_image(path)` - Save image
- `load_image(path)` - Load image

**Image Processing**
- `resize(w, h)` - Resize frame
- `color_convert(code)` - Convert color space
- `gaussian_blur(ksize)` - Gaussian blur
- `canny_edge(low, high)` - Edge detection
- `threshold(val, max, type)` - Thresholding

**Stereo Vision**
- `undistort(params)` - Undistort image
- `stereo_sgbm(left, right)` - SGBM stereo matching
- `apply_colormap(name)` - Apply colormap

**Display**
- `display(title)` - Display frame
- `wait_key(ms)` - Wait for keypress
- `print(msg)` - Print message

**Drawing**
- `draw_text(text, x, y, scale, color)` - Draw text
- `draw_rect(x, y, w, h, color)` - Draw rectangle
- `draw_circle(x, y, r, color)` - Draw circle

## Snippets

| Prefix | Description |
|--------|-------------|
| `pipeline` | Create a pipeline |
| `pipelinep` | Pipeline with parameters |
| `execseq` | Execute sequentially |
| `execmulti` | Execute in parallel |
| `execloop` | Execute in loop |
| `mainpipe` | Main pipeline template |
| `stereopipe` | Stereo pipeline template |
| `videocap` | Video capture |
| `display` | Display frame |
| `drawtext` | Draw text |
| `blur` | Gaussian blur |
| `cvtcolor` | Color conversion |

## Installation

### From VSIX

1. Package the extension:
   ```bash
   cd vscode-vsp
   npx vsce package
   ```

2. Install in VS Code:
   - Press `Ctrl+Shift+P`
   - Type "Install from VSIX"
   - Select the generated `.vsix` file

### Development

1. Open the `vscode-vsp` folder in VS Code
2. Press `F5` to launch Extension Development Host
3. Open a `.vsp` file to test

## License

MIT License
