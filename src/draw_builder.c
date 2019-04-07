#include <stdlib.h>
#include <string.h>

#include "css_func.h"
#include "css_eval.h"
#include "draw_builder.h"
#include "hash.h"
#include "img.h"

#define MAX_ELEMENTS 128

struct Helper {
    struct Program* program;
    struct Rule* rule;
    struct DrawObj *parent;
};

struct DrawObj* _build_triangle(struct Helper* helper);
struct DrawObj* _build_cube(struct Helper* helper);
struct DrawObj* _build_series(struct Helper* helper, enum Series series);

struct HashMap* cache_textures;

void builder_init(void) {
    cache_textures = hash_make();
}

void builder_stop(void) {
    struct image* texture;
    hash_iter_values(texture, cache_textures) {
        destroy_image(texture);
    }
    hash_destroy(cache_textures);
}

struct image* _builder_get_texture(char *filename) {
    struct image* texture;

    texture = hash_get(cache_textures, filename);
    if (texture) {
        return texture;
    }

    texture = read_png_file(filename);
    if (!texture) {
        return NULL;
    }

    hash_set(cache_textures, filename, texture, NULL);
    return texture;
}

struct BasicObj _build_basic(struct Rule* rule) {
    int *width_ptr = css_find_number_prop(rule, "width");
    int *height_ptr = css_find_number_prop(rule, "height");
    int *depth_ptr = css_find_number_prop(rule, "depth");
    int *rotation_ptr = css_find_number_prop(rule, "rotation");

    struct BasicObj basic = {
        .width=width_ptr ? *width_ptr : 50,
        .height=height_ptr ? *height_ptr : 50,
        .depth=depth_ptr ? *depth_ptr : 50,
        .rotation=rotation_ptr ? *rotation_ptr : 0,
    };

    return basic;
}

int _check_el_name(struct RuleSelector *selector, char *name) {
    return strcmp(selector->element, name) == 0;
}

struct DrawObj* _build_draw_obj(struct Helper* helper) {
    struct Helper inner_helper = {
        .program=helper->program,
        .rule=helper->rule,
        .parent=helper->parent,
    };

    struct RuleSelector* query = helper->rule->selector;
    if (_check_el_name(query, "cube")) {
        return _build_cube(&inner_helper);
    } else if (_check_el_name(query, "triangle")) {
        return _build_triangle(&inner_helper);
    } else if (_check_el_name(query, "pyramid")) {
        return NULL;
    } else if (_check_el_name(query, "v-series")) {
        return _build_series(&inner_helper, VERTICAL_SERIES);
    } else if (_check_el_name(query, "h-series")) {
        return _build_series(&inner_helper, HORIZONTAL_SERIES);
    } else if (_check_el_name(query, "d-series")) {
        return _build_series(&inner_helper, DEPTH_SERIES);
    } else {
        return NULL;
    }
}

struct DrawObj** _build_many_objs(struct Helper* helper) {
    struct Rule *rule = helper->rule;
    struct Prop* body_prop = css_find_prop(rule, "body");
    struct Obj* obj = NULL;
    int size = 0;
    css_iter(obj, body_prop->objs) size++; // counter

    struct DrawObj **objs = malloc(sizeof(struct DrawObj*) * (size + 1));
    int i = 0;
    css_iter(obj, body_prop->objs) {
        struct RuleSelector* selector = css_eval_rule(obj);
        if (!selector) continue;
        struct Rule* inner_rule = css_find_rule_by_query(helper->program, selector);
        struct Helper inner_helper = {
            .program=helper->program,
            .rule=inner_rule,
            .parent=helper->parent,
        };
        objs[i++] = _build_draw_obj(&inner_helper);
    }

    objs = realloc(objs, sizeof(struct DrawObj*) * i);
    objs[i] = NULL;

    return objs;
}

struct DrawObj* _build_series(struct Helper* helper, enum Series series) {
    struct Rule *rule = helper->rule;
    int *padding_ptr = css_find_number_prop(rule, "padding");

    struct SeriesObj* obj = malloc(sizeof(struct SeriesObj));
    obj->basic = _build_basic(rule);
    obj->padding = padding_ptr? *padding_ptr : 0;
    obj->series = series;

    struct DrawObj *draw_obj = malloc(sizeof(struct DrawObj));
    draw_obj->type = DRAW_OBJ_SERIES;
    draw_obj->obj = obj;
    draw_obj->parent = helper->parent;

    struct Helper inner_helper = {
        .program=helper->program,
        .rule=helper->rule,
        .parent=draw_obj,
    };
    obj->objs = _build_many_objs(&inner_helper);

    return draw_obj;
}

struct TexObj* _build_texture(struct Helper* helper) {
    struct Rule* rule = helper->rule;
    if (!rule) return NULL;
    char* filename = css_find_string_prop(rule, "texture"); 
    if (!filename) return NULL;

    struct TexObj* obj = malloc(sizeof(struct TexObj));
    obj->texture = filename ? _builder_get_texture(filename) : NULL;
    return obj;
}

int _get_floor_height(int *height_ptr, struct FloorObj* obj) {
    if (height_ptr) {
        return *height_ptr;
    }  
    if (obj->tex && obj->tex->texture) {
        return obj->tex->texture->height;
    } 

    return 0;
}

struct TexObj** _append_objs_to_floor(struct FloorObj* floor, int wall_width) {
    int x = 0;
    int size = 0;
    int is_right = 0;
    int padding = floor->padding;

    struct TexObj** objs = malloc(sizeof(struct TexObj*) * MAX_ELEMENTS);

    struct Helper left_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "left"),
        .parent=helper->parent,
    };
    struct TexObj* left = _build_texture(left_helper);
    if (left && left->width + padding  < wall_width) {
        x += left->width + padding;
        objs[size++] = left;
    }

    struct Helper right_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "right"),
        .parent=helper->parent,
    };
    struct TexObj* right = _build_texture(right_helper);
    if (right && x + right->width < wall_width) {
        is_right = 1;
        wall_width -= right->width;
    }

    for(;;) {
        struct Helper middle_helper = {
            .program=helper->program,
            .rule=css_find_rule_prop(helper->program, rule, "middle"),
            .parent=helper->parent,
        };
        struct TexObj* middle = _build_texture(middle_helper);
        if (!middle) break;
        if (x + middle->width + padding >= wall_width) break;

        x += middle->width + padding;
        objs[size++] = middle;
    }

    if (is_right) {
        objs[size++] = right;
    }


    objs[size] = NULL;
    return objs;
}

struct FloorObj* _build_floor(struct Helper* helper, int wall_width) {
    struct Rule *rule = helper->rule;
    if (!rule) return NULL;
    int* height_ptr = css_find_number_prop(rule, "height");
    int* padding_ptr = css_find_number_prop(rule, "padding");

    struct FloorObj* obj = malloc(sizeof(struct FloorObj));
    obj->padding = padding_ptr ? *padding_ptr : 0;
    obj->height = _get_floor_height(height_ptr, obj);

    struct Helper tex_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "texture"),
        .parent=helper->parent,
    };
    obj->tex = _build_texture(&tex_helper);

    obj->objs = _append_objs_to_floor();

    return obj;
}

struct WallObj* _build_wall(struct Helper* helper, int wall_width, int height_width) {
    struct Rule *rule = helper->rule;
    if (!rule) return NULL;

    struct WallObj *obj = malloc(sizeof(struct WallObj));

    struct Helper tex_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "texture"),
        .parent=helper->parent,
    };
    obj->tex = _build_texture(&tex_helper);
    struct Helper bottom_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "bottom"),
        .parent=helper->parent,
    };
    obj->bottom = _build_floor(&bottom_helper, wall_width);
    struct Helper middle_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "middle"),
        .parent=helper->parent,
    };
    obj->middle = _build_floor(&middle_helper, wall_width);
    struct Helper top_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "top"),
        .parent=helper->parent,
    };
    obj->top = _build_floor(&top_helper, wall_width);
    return obj;
}

struct DrawObj* _build_cube(struct Helper* helper) {
    struct Rule *rule = helper->rule;
    if (!rule) return NULL;
    struct CubeObj* obj = malloc(sizeof(struct CubeObj));
    obj->basic = _build_basic(rule);

    struct Helper wall_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "wall"),
        .parent=helper->parent,
    };
    obj->wall = _build_wall(&wall_helper, obj->basic->width);
    struct Helper roof_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "roof"),
        .parent=helper->parent,
    };
    obj->roof = _build_wall(&roof_helper, obj->basic->width);

    struct DrawObj* draw_obj = malloc(sizeof(struct DrawObj));
    draw_obj->type = DRAW_OBJ_CUBE;
    draw_obj->obj = obj;
    draw_obj->parent = helper->parent;

    return draw_obj;
}

struct DrawObj* _build_triangle(struct Helper* helper) {
    struct Rule *rule = helper->rule;
    if (!rule) return NULL;
    struct TriangleObj* obj = malloc(sizeof(struct TriangleObj));
    obj->basic = _build_basic(rule);

    struct Helper wall_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "wall"),
        .parent=helper->parent,
    };
    obj->wall = _build_wall(&wall_helper);
    struct Helper roof_helper = {
        .program=helper->program,
        .rule=css_find_rule_prop(helper->program, rule, "roof"),
        .parent=helper->parent,
    };
    obj->roof = _build_wall(&roof_helper);

    struct DrawObj* draw_obj = malloc(sizeof(struct DrawObj));
    draw_obj->type = DRAW_OBJ_TRIANGLE;
    draw_obj->obj = obj;
    draw_obj->parent = helper->parent;

    return draw_obj;
}

struct DrawObj* builder_make(struct Program* program, struct Rule* world) {
    struct Rule* body = css_find_rule_prop(program, world, "body");
    struct Helper helper = {
        .program=program,
        .rule=body,
        .parent=NULL,
    };
    return _build_draw_obj(&helper);
}
