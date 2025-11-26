"""
프로퍼티 리플렉션 코드 생성 모듈
BEGIN_PROPERTIES / END_PROPERTIES 블록을 생성합니다.
"""

from jinja2 import Template
from header_parser import ClassInfo, Property


PROPERTY_TEMPLATE = """
BEGIN_PROPERTIES({{ class_name }})
{%- if mark_type == 'SPAWNABLE' %}
    MARK_AS_SPAWNABLE("{{ display_name }}", "{{ description }}")
{%- elif mark_type == 'COMPONENT' %}
    MARK_AS_COMPONENT("{{ display_name }}", "{{ description }}")
{%- endif %}
{%- for prop in properties %}
    {%- if prop.get_property_type_macro() == 'ADD_PROPERTY_RANGE' %}
    ADD_PROPERTY_RANGE({{ prop.type }}, {{ prop.name }}, "{{ prop.category }}", {{ prop.min_value }}f, {{ prop.max_value }}f, {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- elif prop.get_property_type_macro() == 'ADD_PROPERTY_STRUCT_ARRAY' %}
    ADD_PROPERTY_STRUCT_ARRAY({{ prop.metadata.get('struct_type', 'Unknown') }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- elif prop.get_property_type_macro() == 'ADD_PROPERTY_ARRAY' %}
    ADD_PROPERTY_ARRAY({{ prop.metadata.get('inner_type', 'EPropertyType::ObjectPtr') }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- elif prop.get_property_type_macro() == 'ADD_PROPERTY_MAP' %}
    ADD_PROPERTY_MAP({{ prop.metadata.get('key_type', 'EPropertyType::FString') }}, {{ prop.metadata.get('value_type', 'EPropertyType::Int32') }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- elif prop.get_property_type_macro() == 'ADD_PROPERTY_SCRIPT' %}
    ADD_PROPERTY_SCRIPT({{ prop.type }}, {{ prop.name }}, "{{ prop.category }}", "{{ prop.metadata.get('ScriptFile', '.lua') }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- else %}
    {{ prop.get_property_type_macro() }}({{ prop.type }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- endif %}
{%- endfor %}
END_PROPERTIES()
"""


class PropertyGenerator:
    """프로퍼티 등록 코드 생성기"""

    def __init__(self):
        self.template = Template(PROPERTY_TEMPLATE)
        self.all_classes = {}  # 모든 클래스 정보 저장 (name -> ClassInfo)

    def set_all_classes(self, classes):
        """모든 클래스 정보를 설정 (상속 체인 추적용)"""
        self.all_classes = {cls.name: cls for cls in classes}

    def _is_derived_from(self, class_name: str, base_class: str) -> bool:
        """class_name이 base_class를 상속받는지 확인 (재귀적으로 부모 추적)"""
        if class_name == base_class:
            return True

        # 클래스 정보가 없으면 False
        if class_name not in self.all_classes:
            return False

        # 부모 클래스 확인
        parent = self.all_classes[class_name].parent
        if not parent:
            return False

        # 재귀적으로 부모의 부모까지 확인
        return self._is_derived_from(parent, base_class)

    def generate(self, class_info: ClassInfo) -> str:
        """ClassInfo로부터 BEGIN_PROPERTIES 블록 생성"""

        # mark_type 결정:
        # 1. Abstract 클래스는 MARK 없음 (에디터 목록에서 제외)
        # 2. NotSpawnable 클래스는 MARK 없음 (시스템 액터 등)
        # 3. AActor, UActorComponent 베이스 클래스는 MARK 없음
        # 4. AActor를 상속받은 클래스는 MARK_AS_SPAWNABLE
        # 5. UActorComponent를 상속받은 클래스는 MARK_AS_COMPONENT
        # 6. 그 외 (순수 UObject 등)는 MARK 없음
        mark_type = None
        if class_info.is_abstract:
            mark_type = None  # Abstract 클래스는 MARK 없음
        elif getattr(class_info, 'not_spawnable', False):
            mark_type = None  # NotSpawnable 클래스는 MARK 없음 (시스템 액터)
        elif class_info.name in ['AActor', 'UActorComponent']:
            mark_type = None  # 베이스 클래스는 MARK 없음
        elif self._is_derived_from(class_info.name, 'AActor'):
            mark_type = 'SPAWNABLE'  # AActor를 상속받은 클래스 (직간접)
        elif self._is_derived_from(class_info.name, 'UActorComponent'):
            mark_type = 'COMPONENT'  # UActorComponent를 상속받은 클래스 (직간접)
        # else: mark_type은 None으로 유지 (순수 UObject 등)

        # DisplayName과 Description 결정
        display_name = class_info.display_name or class_info.name
        description = class_info.description or f"Auto-generated {class_info.name}"

        if not class_info.properties:
            # 프로퍼티가 없어도 기본 블록은 생성
            if mark_type == 'SPAWNABLE':
                mark_line = f'    MARK_AS_SPAWNABLE("{display_name}", "{description}")'
            elif mark_type == 'COMPONENT':
                mark_line = f'    MARK_AS_COMPONENT("{display_name}", "{description}")'
            else:
                mark_line = ''

            return f"""
BEGIN_PROPERTIES({class_info.name})
{mark_line}
END_PROPERTIES()
"""

        return self.template.render(
            class_name=class_info.name,
            mark_type=mark_type,
            display_name=class_info.display_name or class_info.name,
            description=class_info.description or f"Auto-generated {class_info.name}",
            properties=class_info.properties
        )

    def generate_struct(self, struct_info) -> str:
        """StructInfo로부터 BEGIN_STRUCT_PROPERTIES 블록 생성"""
        from jinja2 import Template

        STRUCT_TEMPLATE = """
BEGIN_STRUCT_PROPERTIES({{ struct_name }})
{%- for prop in properties %}
    {%- if prop.get_property_type_macro() == 'ADD_PROPERTY_RANGE' %}
    ADD_PROPERTY_RANGE({{ prop.type }}, {{ prop.name }}, "{{ prop.category }}", {{ prop.min_value }}f, {{ prop.max_value }}f, {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- elif prop.get_property_type_macro() == 'ADD_PROPERTY_STRUCT_ARRAY' %}
    ADD_PROPERTY_STRUCT_ARRAY({{ prop.metadata.get('struct_type', 'Unknown') }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- elif prop.get_property_type_macro() == 'ADD_PROPERTY_ARRAY' %}
    ADD_PROPERTY_ARRAY({{ prop.metadata.get('inner_type', 'EPropertyType::ObjectPtr') }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- elif prop.get_property_type_macro() == 'ADD_PROPERTY_MAP' %}
    ADD_PROPERTY_MAP({{ prop.metadata.get('key_type', 'EPropertyType::FString') }}, {{ prop.metadata.get('value_type', 'EPropertyType::Int32') }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- else %}
    {{ prop.get_property_type_macro() }}({{ prop.type }}, {{ prop.name }}, "{{ prop.category }}", {{ 'true' if prop.editable else 'false' }}{% if prop.tooltip %}, "{{ prop.tooltip }}"{% endif %})
    {%- endif %}
{%- endfor %}
END_PROPERTIES()
"""
        template = Template(STRUCT_TEMPLATE)

        if not struct_info.properties:
            return f"""
BEGIN_STRUCT_PROPERTIES({struct_info.name})
END_PROPERTIES()
"""

        return template.render(
            struct_name=struct_info.name,
            properties=struct_info.properties
        )
