"""
C++ 헤더 파일 파싱 모듈
UPROPERTY와 UFUNCTION 매크로를 찾아서 파싱합니다.
"""

import re
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Optional


@dataclass
class Property:
    """프로퍼티 정보"""
    name: str
    type: str
    category: str = ""
    editable: bool = True
    tooltip: str = ""
    min_value: float = 0.0
    max_value: float = 0.0
    has_range: bool = False
    metadata: Dict[str, str] = field(default_factory=dict)

    def get_property_type_macro(self) -> str:
        """타입에 맞는 ADD_PROPERTY 매크로 결정"""
        type_lower = self.type.lower()

        # 포인터 타입 체크
        if '*' in self.type:
            if 'utexture' in type_lower:
                return 'ADD_PROPERTY_TEXTURE'
            elif 'ustaticmesh' in type_lower:
                return 'ADD_PROPERTY_STATICMESH'
            elif 'umaterial' in type_lower:
                return 'ADD_PROPERTY_MATERIAL'
            elif 'usound' in type_lower:
                return 'ADD_PROPERTY_AUDIO'
            else:
                return 'ADD_PROPERTY'

        # TArray 타입 체크
        if 'tarray' in type_lower:
            # TArray<UMaterialInterface*> 같은 형태에서 내부 타입 추출
            match = re.search(r'tarray\s*<\s*(\w+)', self.type, re.IGNORECASE)
            if match:
                inner_type = match.group(1).lower()
                if 'umaterial' in inner_type:
                    self.metadata['inner_type'] = 'EPropertyType::Material'
                elif 'utexture' in inner_type:
                    self.metadata['inner_type'] = 'EPropertyType::Texture'
                elif 'usound' in inner_type:
                    self.metadata['inner_type'] = 'EPropertyType::Sound'
                elif 'ustaticmesh' in inner_type:
                    self.metadata['inner_type'] = 'EPropertyType::StaticMesh'
                else:
                    self.metadata['inner_type'] = 'EPropertyType::ObjectPtr'
            return 'ADD_PROPERTY_ARRAY'

        # 범위가 있는 프로퍼티
        if self.has_range:
            return 'ADD_PROPERTY_RANGE'

        return 'ADD_PROPERTY'


@dataclass
class Parameter:
    """함수 파라미터 정보"""
    name: str
    type: str


@dataclass
class Function:
    """함수 정보"""
    name: str
    display_name: str
    return_type: str
    parameters: List[Parameter] = field(default_factory=list)
    is_const: bool = False
    metadata: Dict[str, str] = field(default_factory=dict)

    def get_parameter_types_string(self) -> str:
        """템플릿 파라미터 문자열 생성: <UClass, uint32, const FString&, ...>"""
        if not self.parameters:
            return ""
        param_types = ", ".join([p.type for p in self.parameters])
        return f", {param_types}"


@dataclass
class ClassInfo:
    """클래스 정보"""
    name: str
    parent: str
    header_file: Path
    properties: List[Property] = field(default_factory=list)
    functions: List[Function] = field(default_factory=list)
    is_component: bool = False
    is_spawnable: bool = False
    display_name: str = ""
    description: str = ""


class HeaderParser:
    """C++ 헤더 파일 파서"""

    # 정규식 패턴
    UPROPERTY_PATTERN = re.compile(
        r'UPROPERTY\s*\((.*?)\)\s*'
        r'(.*?)\s+(\w+)\s*[;=]',
        re.DOTALL
    )

    UFUNCTION_PATTERN = re.compile(
        r'UFUNCTION\s*\((.*?)\)\s*'
        r'(.*?)\s+(\w+)\s*\((.*?)\)\s*(const)?\s*[;{]',
        re.DOTALL
    )

    CLASS_PATTERN = re.compile(
        r'class\s+(\w+)\s*:\s*public\s+(\w+)'
    )

    GENERATED_REFLECTION_PATTERN = re.compile(
        r'GENERATED_REFLECTION_BODY\(\)'
    )

    def parse_header(self, header_path: Path) -> Optional[ClassInfo]:
        """헤더 파일 파싱"""
        content = header_path.read_text(encoding='utf-8')

        # GENERATED_REFLECTION_BODY() 체크
        if not self.GENERATED_REFLECTION_PATTERN.search(content):
            return None

        # 클래스 정보 추출
        class_match = self.CLASS_PATTERN.search(content)
        if not class_match:
            return None

        class_name = class_match.group(1)
        parent_name = class_match.group(2)

        class_info = ClassInfo(
            name=class_name,
            parent=parent_name,
            header_file=header_path
        )

        # UPROPERTY 파싱
        for match in self.UPROPERTY_PATTERN.finditer(content):
            metadata_str = match.group(1)
            prop_type = match.group(2).strip()
            prop_name = match.group(3)

            prop = self._parse_property(prop_name, prop_type, metadata_str)
            class_info.properties.append(prop)

        # UFUNCTION 파싱
        for match in self.UFUNCTION_PATTERN.finditer(content):
            metadata_str = match.group(1)
            return_type = match.group(2).strip()
            func_name = match.group(3)
            params_str = match.group(4)
            is_const = match.group(5) is not None

            func = self._parse_function(
                func_name, return_type, params_str,
                metadata_str, is_const
            )
            class_info.functions.append(func)

        return class_info

    def _parse_property(self, name: str, type_str: str, metadata: str) -> Property:
        """프로퍼티 메타데이터 파싱"""
        prop = Property(name=name, type=type_str)

        # Category 추출
        category_match = re.search(r'Category\s*=\s*"([^"]+)"', metadata)
        if category_match:
            prop.category = category_match.group(1)

        # EditAnywhere 체크
        prop.editable = 'EditAnywhere' in metadata

        # Range 추출
        range_match = re.search(r'Range\s*=\s*"([^"]+)"', metadata)
        if range_match:
            range_str = range_match.group(1)
            min_max = range_str.split(',')
            if len(min_max) == 2:
                prop.has_range = True
                prop.min_value = float(min_max[0].strip())
                prop.max_value = float(min_max[1].strip())

        # Tooltip 추출
        tooltip_match = re.search(r'Tooltip\s*=\s*"([^"]+)"', metadata)
        if tooltip_match:
            prop.tooltip = tooltip_match.group(1)

        return prop

    def _parse_function(
        self, name: str, return_type: str,
        params_str: str, metadata: str, is_const: bool
    ) -> Function:
        """함수 메타데이터 파싱"""
        func = Function(
            name=name,
            display_name=name,
            return_type=return_type,
            is_const=is_const
        )

        # DisplayName 추출
        display_match = re.search(r'DisplayName\s*=\s*"([^"]+)"', metadata)
        if display_match:
            func.display_name = display_match.group(1)

        # LuaBind 체크
        func.metadata['lua_bind'] = 'LuaBind' in metadata

        # 파라미터 파싱
        if params_str.strip():
            params = [p.strip() for p in params_str.split(',')]
            for param in params:
                # "const FString& Name" -> type="const FString&", name="Name"
                parts = param.rsplit(None, 1)
                if len(parts) == 2:
                    param_type = parts[0]
                    param_name = parts[1]
                    func.parameters.append(Parameter(name=param_name, type=param_type))

        return func

    def find_reflection_classes(self, source_dir: Path) -> List[ClassInfo]:
        """소스 디렉토리에서 GENERATED_REFLECTION_BODY가 있는 모든 클래스 찾기"""
        classes = []

        for header in source_dir.rglob("*.h"):
            try:
                class_info = self.parse_header(header)
                if class_info:
                    classes.append(class_info)
                    print(f"✓ Found reflection class: {class_info.name} in {header.name}")
            except Exception as e:
                print(f"✗ Error parsing {header}: {e}")

        return classes
