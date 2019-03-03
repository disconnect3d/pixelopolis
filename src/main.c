#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "basic.h"
#include "img.h"
#include "draw.h"
#include "draw_shape.h"
#include "css_func.h"
#include "css_draw.h"

#include "css_debug.h"

void do_sth(struct image *img) {
    struct Program *program = css_parse_file("test.css");
    int avox[3] = {16, 32, 64};

    struct RuleSelector query = default_selector;
    query.element = "cube";
    query.klass = "road";

    struct DrawInfo draw_info = {
        .img=img,
        .program=program,
        .query=&query,
        .vox=avox,
    };
    css_draw_cube(&draw_info);

    int vox[3] = {32, 64, 64};
    query = default_selector;
    query.element = "series";

    struct Rule* series = css_find_rule_by_query(program, &query);
    if (!series) return;
    struct Prop* series_prop = css_find_prop(series, "buildings");
    if (!series_prop) return;
    struct Obj* obj = NULL;

    int *padding_ptr = css_find_number_prop(series, "padding");
    int padding = padding_ptr ? *padding_ptr : 8;

    css_iter(obj, series_prop->objs) {
        if (obj->type != OBJ_RULE) continue;
        struct RuleSelector *building_query = obj->value;
        struct Rule* building = css_find_rule_by_query(program, building_query);
        if (strcmp(building->selector->element, "cube") == 0) {
            struct DrawInfo draw_info = {
                .img=img,
                .program=program,
                .query=building->selector,
                .vox=vox,
            };
            css_draw_cube(&draw_info);
            vox[0] += *css_find_number_prop(building, "depth") + padding;
        }
    }
}

int main(int argc, char **argv) {
    struct image *img = create_black_image(400, 400);
    do_sth(img);
    write_png_file("out.png", img);
    return 0;
}
