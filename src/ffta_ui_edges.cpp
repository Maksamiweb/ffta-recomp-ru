#include "ffta_ui_edges.h"
#include "ffta_speed_boost.h"
#include "gba_bus.h"
#include "gba_ppu.h"
#include "host_overlay.h"
#include "runtime_bus_bridge.h"

namespace ffta {
void presentation_overlay(gbarecomp::HostOverlay* overlay) {
    auto* bus = gbarecomp::active_bus();
    auto* ppu = gbarecomp::active_ppu();
    if (overlay && bus && ppu && ppu->has_latched_framebuffer() &&
        !gba::g_ws_pillarbox && ppu->render_height() == 160 &&
        int(ppu->render_width()) == overlay->view_width()) {
        const int left = gba::g_ws_native_view_left >= 0 ? gba::g_ws_native_view_left : int(ppu->view_extra_left());
        for (int side = 0; side < 2; ++side) {
            const int margin = side ? overlay->view_width() - left - 240 : left;
            if (margin < 3) continue;
            bool rows[160];
            ui_edge_runs(bus->io().raw(), bus->vram_ptr(), bus->pal_ptr(),
                         ppu->latched_framebuffer(), overlay->view_width(), left, side, rows);
            for (int start = 0; start < 160;) {
                if (!rows[start]) { ++start; continue; }
                int end = start;
                while (end < 160 && rows[end]) ++end;
                const float edge = float(side ? left + 240 : left);
                const float outer = side ? edge + 1 : edge - 2;
                const float inner = side ? edge : edge - 1;
                using gbarecomp::OverlaySpace;
                overlay->fill_rect(outer, float(start), 1, float(end - start), 0,
                                   {49, 42, 24, 255}, OverlaySpace::View);
                overlay->fill_rect(inner, float(start), 1, float(end - start), 0,
                                   {156, 144, 91, 255}, OverlaySpace::View);
                start = end;
            }
        }
    }
    speed_boost_overlay(overlay);
}
} // namespace ffta
