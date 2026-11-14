#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace canvas
{
struct Image
{
    int width = 320;
    int height = 200;
    std::vector<int> pixels = std::vector<int>(320 * 200, 0);

    static Image blank(int width, int height)
    {
        if (width < 1 || height < 1 || width > 2048 || height > 2048)
            throw std::invalid_argument("Image dimensions must be between 1 and 2048 pixels");
        return {width, height, std::vector<int>(static_cast<std::size_t>(width) * height, 0)};
    }
    bool operator==(const Image &) const = default;
};

// A stroke or a complete filter is one transaction. Preview and cancellation
// use the same transaction, so partially processed images never become edits.
class Document
{
    struct State
    {
        Image image;
        std::uint64_t revision = 0;
    };
    State m_current;
    std::optional<State> m_before;
    std::deque<State> m_undo, m_redo;
    std::uint64_t m_nextRevision = 1, m_savedRevision = 0;
    std::size_t m_historyLimit;

    void trimHistory()
    {
        auto bytes = [](const auto &states)
        {
            std::size_t result = 0;
            for (const auto &state : states)
                result += state.image.pixels.size() * sizeof(int);
            return result;
        };
        while (bytes(m_undo) + bytes(m_redo) > m_historyLimit)
        {
            if (!m_undo.empty())
                m_undo.pop_front();
            else if (!m_redo.empty())
                m_redo.pop_front();
            else
                break;
        }
    }

public:
    explicit Document(std::size_t historyLimit = 32 * 1024 * 1024) : m_historyLimit(historyLimit) {}
    Image &image() { return m_current.image; }
    const Image &image() const { return m_current.image; }
    bool editing() const { return m_before.has_value(); }
    bool dirty() const
    {
        return m_current.revision != m_savedRevision || (m_before && m_current.image != m_before->image);
    }
    bool canUndo() const { return !editing() && !m_undo.empty(); }
    bool canRedo() const { return !editing() && !m_redo.empty(); }
    void beginEdit()
    {
        if (editing())
            throw std::logic_error("An image edit is already in progress");
        m_before = m_current;
    }
    bool commitEdit()
    {
        if (!editing())
            return false;
        const bool changed = m_current.image != m_before->image;
        if (changed)
        {
            m_undo.push_back(std::move(*m_before));
            m_redo.clear();
            m_current.revision = m_nextRevision++;
        }
        m_before.reset();
        trimHistory();
        return changed;
    }
    void cancelEdit()
    {
        if (m_before)
        {
            m_current = std::move(*m_before);
            m_before.reset();
        }
    }
    void restartPreview()
    {
        if (!m_before)
            throw std::logic_error("No preview is in progress");
        m_current = *m_before;
    }
    bool undo()
    {
        if (!canUndo())
            return false;
        m_redo.push_back(std::move(m_current));
        m_current = std::move(m_undo.back());
        m_undo.pop_back();
        return true;
    }
    bool redo()
    {
        if (!canRedo())
            return false;
        m_undo.push_back(std::move(m_current));
        m_current = std::move(m_redo.back());
        m_redo.pop_back();
        return true;
    }
    void replace(Image image, bool saved)
    {
        const auto count = static_cast<std::size_t>(image.width) * image.height;
        if (image.width < 1 || image.height < 1 || image.width > 2048 || image.height > 2048 ||
            image.pixels.size() != count)
            throw std::invalid_argument("Invalid image dimensions or pixel count");
        m_current = {std::move(image), m_nextRevision++};
        m_before.reset();
        m_undo.clear();
        m_redo.clear();
        if (saved)
            markSaved();
    }
    void markSaved()
    {
        if (editing())
            throw std::logic_error("Finish the edit before saving");
        m_savedRevision = m_current.revision;
    }
};
} // namespace canvas
