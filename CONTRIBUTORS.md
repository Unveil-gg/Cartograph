# Contributing to Cartograph

Thank you for your interest in contributing to Cartograph! We appreciate 
your time and effort in helping make this tool better for the game 
development community.

## Code Style

All C++ code should follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). We, also, target C++17.

Key points:
- Maximum 80 characters per line (100 hard limit)
- Use `.clang-format` for consistent formatting
- Comment all functions with brief usage, parameters, and return values
- Prefer simplicity and avoid code duplication

## Working on Features

**Please work on features requested in existing Issues first.** This helps 
ensure your contribution aligns with the project's direction and needs.

If you'd like to add a new feature:
1. **Open an issue first** describing the feature and your proposed approach
2. Wait for maintainer feedback before starting implementation
3. Reference the issue number in your pull request

This prevents duplicate work and ensures the feature fits the project's 
goals.

## Bug Reports

Good bug reports are essential. Please include:

1. **Situation**: What you were doing when the bug occurred
2. **Platform**: Operating system and version (e.g., macOS 13.2, Windows 11)
3. **Steps to reproduce**: Clear, numbered steps
4. **Expected vs actual behavior**: What should happen vs what happened
5. **Root cause** (if known): Any investigation you've done

Example:
```
**Platform**: macOS 13.2.1  
**Situation**: Exporting PNG with 4x scale and all layers enabled
**Steps**: 
  1. Create project with 10+ rooms
  2. File > Export PNG
  3. Select 4x scale, enable all layers
  4. Click Export
**Expected**: PNG exports to selected location
**Actual**: App crashes with segfault
**Potential root cause**: May be related to memory allocation in 
ExportPng.cpp line 142
```

## Testing

**Please test on both macOS and Windows** before submitting pull requests. 
Cross-platform compatibility is a core goal of this project.

If you only have access to one platform, mention this in your PR and 
request testing help.

If a PR targets a fix specific to a platform, mention this to waive this requirement.

## Pull Request Process

1. Fork the repository and create a feature branch
2. Make your changes following the style guide
3. Test on both platforms when possible
4. Write clear commit messages
5. Submit PR referencing the related issue
6. Be responsive to code review feedback

## Questions?

Feel free to ask questions in Issues or Discussions. We're here to help!

---

Thank you for contributing to Cartograph! 🗺️
