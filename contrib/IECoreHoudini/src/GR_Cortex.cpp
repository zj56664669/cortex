//////////////////////////////////////////////////////////////////////////
//
//  Copyright 2010 Dr D Studios Pty Limited (ACN 127 184 954) (Dr. D Studios),
//  its affiliates and/or its licensors.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

// OpenGL
#include <glew.h>

// Houdini
#include <UT/UT_Version.h>
#include <RE/RE_Render.h>
#include <UT/UT_Interrupt.h>
#include <GB/GB_AttributeRef.h>

// Cortex
#include <IECoreGL/Scene.h>
#include <IECoreGL/State.h>
#include <IECoreGL/StateComponent.h>
#include <IECoreGL/TypedStateComponent.h>
#include <IECoreGL/NameStateComponent.h>
#include <IECoreGL/BoxPrimitive.h>
#include <IECoreGL/Exception.h>
#include <IECoreGL/Group.h>
#include <IECoreGL/Camera.h>
#include <IECore/SimpleTypedData.h>

// IECoreHoudini
#include "GR_Cortex.h"
#include "SOP_OpHolder.h"
#include "SOP_ProceduralHolder.h"
#include "NodePassData.h"
using namespace IECoreHoudini;

// ctor
GR_Cortex::GR_Cortex()
{
	IECoreGL::init( true );
}
// dtor
GR_Cortex::~GR_Cortex()
{
}

// Tell Houdini to only render GU_ProceduralDetails with this
// render hook.
int GR_Cortex::getWireMask( GU_Detail *gdp,
		const GR_DisplayOption *dopt
		) const
{
	if ( gdp->attribs().find("IECoreHoudini::NodePassData", GB_ATTRIB_MIXED) )
	{
		return 0;
	}
	else
	{
    	return GEOPRIMALL;
	}
}

// Tell Houdini to only render GU_ProceduralDetails with this
// render hook.
int GR_Cortex::getShadedMask( GU_Detail *gdp,
		const GR_DisplayOption *dopt
		) const
{
	if ( gdp->attribs().find("IECoreHoudini::NodePassData", GB_ATTRIB_MIXED) )
	{
		return 0;
	}
	else
	{
    	return GEOPRIMALL;
	}
}

// Render our ParameterisedProcedural in wireframe
void GR_Cortex::renderWire( GU_Detail *gdp,
    RE_Render &ren,
    const GR_AttribOffset &ptinfo,
    const GR_DisplayOption *dopt,
    float lod,
    const GU_PrimGroupClosure *hidden_geometry
    )
{
    // our render state
    IECoreGL::ConstStatePtr displayState = getDisplayState( dopt, true );
    render( gdp, displayState );
}

// Render our ParameterisedProcedural in shaded
void GR_Cortex::renderShaded( GU_Detail *gdp,
		RE_Render &ren,
		const GR_AttribOffset &ptinfo,
		const GR_DisplayOption *dopt,
		float lod,
		const GU_PrimGroupClosure *hidden_geometry
		)
{
    // our render state
    IECoreGL::ConstStatePtr displayState = getDisplayState( dopt, false );
    render( gdp, displayState );
}

// Get a Cortex display state based on the Houdini display options
IECoreGL::ConstStatePtr GR_Cortex::getDisplayState(
		const GR_DisplayOption *dopt,
		bool wireframe
		)
{
	// default is good for shaded
	IECoreGL::StatePtr state = new IECoreGL::State( true );

	// add some properties for wireframe rendering
	if ( wireframe )
	{
		state->add( new IECoreGL::Primitive::DrawSolid( false ) );
		state->add( new IECoreGL::Primitive::DrawWireframe( true ) );
		UT_Color wire_col;
		wire_col = dopt->wireColor();
		float r,g,b;
		wire_col.getValue( r, g, b );
		state->add( new IECoreGL::WireframeColorStateComponent( Imath::Color4f( r, g, b, 1 ) ) );
	}
	return state;
}

// Renders an OpenGL scene (normally from a parameterisedprocedural)
void GR_Cortex::renderScene( IECoreGL::ConstScenePtr scene, IECoreGL::ConstStatePtr displayState )
{
    // render our scene
	GLint prevProgram;
	glGetIntegerv( GL_CURRENT_PROGRAM, &prevProgram );
	scene->root()->render( displayState );
	glUseProgram( prevProgram );
}

// Renders an object directly (nbrmally from an opHolder)
void GR_Cortex::renderObject( const IECore::Object *object, IECoreGL::ConstStatePtr displayState )
{
	// try and cast this to a visible renderable
	IECore::ConstVisibleRenderablePtr renderable = IECore::runTimeCast<const IECore::VisibleRenderable>( object );
	if ( !renderable )
		return;

    // render our object into a buffer
	IECoreGL::RendererPtr renderer = new IECoreGL::Renderer();
	renderer->setOption( "gl:mode", new IECore::StringData( "deferred" ) );
	renderer->worldBegin();
	renderable->render( renderer );
	renderer->worldEnd();
	IECoreGL::ConstScenePtr scene = renderer->scene();

	// now render
	GLint prevProgram;
	glGetIntegerv( GL_CURRENT_PROGRAM, &prevProgram );
	scene->root()->render( displayState );
	glUseProgram( prevProgram );
}

// general cortex render function, takes a gu_detail and uses the NodePassData attribute
// to call the required render method
void GR_Cortex::render( GU_Detail *gdp, IECoreGL::ConstStatePtr displayState )
{
    // gl scene from a parameterised procedural
    if ( gdp->attribs().find("IECoreHoudini::NodePassData", GB_ATTRIB_MIXED) )
    {
    	GB_AttributeRef attrOffset = gdp->attribs().getOffset( "IECoreHoudini::NodePassData", GB_ATTRIB_MIXED );
    	NodePassData *pass_data = gdp->attribs().castAttribData<NodePassData>( attrOffset );

    	switch( pass_data->type() )
    	{
    		case IECoreHoudini::NodePassData::CORTEX_OPHOLDER:
    		{
    			SOP_OpHolder *sop = dynamic_cast<SOP_OpHolder*>( const_cast<OP_Node*>( pass_data->nodePtr() ) );
				if ( !sop )
					return;
				IECore::OpPtr op = IECore::runTimeCast<IECore::Op>( sop->getParameterised() );
				if ( !op )
					return;
				const IECore::Parameter *result_parameter = op->resultParameter();
				const IECore::Object *result_object = result_parameter->getValue();
				renderObject( result_object, displayState );
    			break;
    		}

    		case IECoreHoudini::NodePassData::CORTEX_PROCEDURALHOLDER:
    		{
    			SOP_ProceduralHolder *sop = dynamic_cast<SOP_ProceduralHolder*>( const_cast<OP_Node*>( pass_data->nodePtr() ) );
    			if ( !sop )
    				return;
    	    	IECoreGL::ConstScenePtr scene = sop->scene();
    	    	if ( !scene )
    	    		return;
    	    	renderScene( scene, displayState );
    			break;
    		}

    		default:
    			break;
    	}
    }
}