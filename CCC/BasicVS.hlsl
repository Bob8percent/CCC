struct Output
{
    float4 pos : POSITION;
    float4 svpos : SV_POSITION;
};

Output BasicVS( float4 pos : POSITION )
{
    Output _out;
    _out.pos = pos;
    _out.svpos = pos;
	return _out;
}