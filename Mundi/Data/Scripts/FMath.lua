function GlobalConfig.Dot(A, B)
    return A.X * B.X + A.Y * B.Y + A.Z * B.Z
end

function GlobalConfig.Cross(A, B)
    return Vector(
        A.Y * B.Z - A.Z * B.Y,
        A.Z * B.X - A.X * B.Z,
        A.X * B.Y - A.Y * B.X
    )
end


function GlobalConfig.Length(V)
    return math.sqrt(GlobalConfig.Dot(V, V))
end

function GlobalConfig.Normalize(V)
    local L = GlobalConfig.Length(V)
    if L > 1e-6 then 
        return Vector(V.X / L, V.Y / L, V.Z / L)
    end 

    return Vector(0,0,0)
end

