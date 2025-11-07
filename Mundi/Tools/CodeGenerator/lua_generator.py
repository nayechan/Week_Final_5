"""
Lua 바인딩 코드 생성 모듈
LUA_BIND_BEGIN / LUA_BIND_END 블록을 생성합니다.
"""

from jinja2 import Template
from parser import ClassInfo, Function


LUA_BIND_TEMPLATE = """
extern "C" void LuaBind_Anchor_{{ class_name }}() {}

LUA_BIND_BEGIN({{ class_name }})
{
{%- for func in functions %}
    {%- if func.return_type == 'void' %}
    AddAlias<{{ class_name }}{{ func.get_parameter_types_string() }}>(
        T, "{{ func.display_name }}", &{{ class_name }}::{{ func.name }});
    {%- else %}
    AddMethodR<{{ func.return_type }}, {{ class_name }}{{ func.get_parameter_types_string() }}>(
        T, "{{ func.display_name }}", &{{ class_name }}::{{ func.name }});
    {%- endif %}
{%- endfor %}
}
LUA_BIND_END()
"""


class LuaBindingGenerator:
    """Lua 바인딩 코드 생성기"""

    def __init__(self):
        self.template = Template(LUA_BIND_TEMPLATE)

    def generate(self, class_info: ClassInfo) -> str:
        """ClassInfo로부터 LUA_BIND_BEGIN 블록 생성"""
        # LuaBind 메타데이터가 있는 함수만 필터링
        lua_functions = [
            f for f in class_info.functions
            if f.metadata.get('lua_bind', False)
        ]

        if not lua_functions:
            # 바인딩할 함수가 없으면 빈 블록
            return f"""
extern "C" void LuaBind_Anchor_{class_info.name}() {{}}

LUA_BIND_BEGIN({class_info.name})
{{
    // No functions to bind
}}
LUA_BIND_END()
"""

        return self.template.render(
            class_name=class_info.name,
            functions=lua_functions
        )
