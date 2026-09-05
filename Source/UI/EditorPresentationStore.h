#pragma once

#include <utility>

#include "UI/EditorState.h"

class EditorPresentationStore
{
public:
    bool apply(const EditorUiSnapshot& next)
    {
        const bool changed = !m_hasSnapshot
            || next.configurationRevision != m_snapshot.configurationRevision
            || next.sessionRevision != m_snapshot.sessionRevision;
        m_snapshot = next;
        m_hasSnapshot = true;
        return changed;
    }

    const EditorUiSnapshot& snapshot() const { return m_snapshot; }
    bool hasSnapshot() const { return m_hasSnapshot; }

private:
    EditorUiSnapshot m_snapshot;
    bool m_hasSnapshot = false;
};
