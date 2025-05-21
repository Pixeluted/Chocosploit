//
// Created by Pixeluted on 03/05/2025.
//

#include "Drawing.hpp"

#include <memory>

#include "../../Managers/Render/RenderManager.hpp"

int new_drawing(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    const std::string newObjectType = lua_tostring(L, 1);
    if (newObjectType != "Line" && newObjectType != "Circle" && newObjectType != "Square" && newObjectType != "Quad" &&
        newObjectType != "Triangle" && newObjectType != "Text" && newObjectType != "Image")
        luaL_argerrorL(L, 1, "Invalid drawing type");

    const auto drawing = std::make_shared<DrawingObj>();
    drawing->isVisible = true;
    drawing->transparency = 1;
    drawing->color = ImVec4{1.0f, 1.0f, 1.0f, 1.0f};
    drawing->zIndex = 1;

    if (newObjectType == "Line") {
        drawing->objectType = Line;
        drawing->line.thickness = 1.0f;
    } else if (newObjectType == "Circle") {
        drawing->objectType = Circle;
        drawing->circle.numsides = 12;
        drawing->circle.radius = 5.0f;
        drawing->circle.position = ImVec2{500.0f, 500.0f};
        drawing->circle.thickness = 1.0f;
        drawing->circle.filled = false;
    } else if (newObjectType == "Square") {
        drawing->objectType = Square;
        drawing->square.position = ImVec2{500.0f, 500.0f};
        drawing->square.size = ImVec2{1.0f, 1.0f};
        drawing->square.thickness = 1.0f;
        drawing->square.filled = false;
    } else if (newObjectType == "Quad") {
        drawing->objectType = Quad;
        drawing->quad.pointA = ImVec2{0.0f, 0.0f};
        drawing->quad.pointB = ImVec2{0.0f, 0.0f};
        drawing->quad.pointC = ImVec2{0.0f, 0.0f};
        drawing->quad.pointD = ImVec2{0.0f, 0.0f};
        drawing->quad.thickness = 1.0f;
        drawing->quad.filled = false;
    } else if (newObjectType == "Triangle") {
        drawing->objectType = Triangle;
        drawing->triangle.pointA = ImVec2{0.0f, 0.0f};
        drawing->triangle.pointB = ImVec2{0.0f, 0.0f};
        drawing->triangle.pointC = ImVec2{0.0f, 0.0f};
        drawing->triangle.thickness = 1.0f;
        drawing->triangle.filled = false;
    } else if (newObjectType == "Text") {
        drawing->objectType = Text;
        drawing->text.font = UI;
        drawing->text.text = "Hello World";
        drawing->text.position = ImVec2{500.0f, 500.0f};
        drawing->text.size = 5.0f;
        drawing->text.center = false;
        drawing->text.outline = false;
        drawing->text.outlineColor = ImVec4{0, 0, 0, 1.0f};
    } else if (newObjectType == "Image") {
        drawing->objectType = Image;
        drawing->image.position = ImVec2{500.0f, 500.0f};
    }

    ApplicationContext::GetService<RenderManager>()->PushDrawObject(drawing);
    drawing->BuildLuaObject(L, drawing);

    return 1;
}

int getalldrawobjects(lua_State *L) {
    const auto manager = ApplicationContext::GetService<RenderManager>();

    int currentIndex = 1;
    lua_newtable(L);
    for (const auto &renderObj: manager->activeDrawingObjects) {
        renderObj->BuildLuaObject(L, renderObj);

        lua_rawseti(L, -2, currentIndex++);
    }

    return 1;
}

int cleardrawcache(lua_State *L) {
    ApplicationContext::GetService<RenderManager>()->ClearAllObjects();
    return 0;
}

int getrenderproperty(lua_State *L) {
    CheckExploitObject(L, 1, DrawingObject);
    luaL_checktype(L, 2, LUA_TSTRING);

    lua_pushvalue(L, 1);
    lua_getfield(L, -1, lua_tostring(L, 2));
    return 1;
}

int setrenderproperty(lua_State *L) {
    CheckExploitObject(L, 1, DrawingObject);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checkany(L, 3);

    lua_pushvalue(L, 1);
    lua_pushvalue(L, 3);
    lua_setfield(L, -2, lua_tostring(L, 2));
    return 0;
}

int isrenderobj(lua_State *L) {
    lua_pushboolean(L, IsSpecificExploitObject(L, 1, DrawingObject));
    return 1;
}

std::vector<RegistrationType> Drawing::GetRegistrationTypes() {
    return {
        RegisterIntoGlobals{},
        RegisterIntoTable{"Drawing"}
    };
}

luaL_Reg *Drawing::GetFunctions() {
     const auto Drawing_functions = new luaL_Reg[] {
         {"new_drawing", new_drawing},
         {"getalldrawobjects", getalldrawobjects},
         {"cleardrawcache", cleardrawcache},
         {"getrenderproperty", getrenderproperty},
         {"setrenderproperty", setrenderproperty},
         {"isrenderobj", isrenderobj},
         {nullptr, nullptr}
     };

     return Drawing_functions;
}
