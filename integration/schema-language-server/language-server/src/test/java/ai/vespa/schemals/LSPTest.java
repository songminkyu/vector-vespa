package ai.vespa.schemals;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;

import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.lsp4j.Hover;
import org.eclipse.lsp4j.Location;
import org.eclipse.lsp4j.Position;
import org.eclipse.lsp4j.Range;
import org.eclipse.lsp4j.TextDocumentItem;
import org.junit.jupiter.api.Test;

import com.yahoo.collections.Pair;
import com.yahoo.io.IOUtils;

import ai.vespa.schemals.common.ClientLogger;
import ai.vespa.schemals.context.EventPositionContext;
import ai.vespa.schemals.context.InvalidContextException;
import ai.vespa.schemals.context.ParseContext;
import ai.vespa.schemals.index.SchemaIndex;
import ai.vespa.schemals.lsp.schema.definition.SchemaDefinition;
import ai.vespa.schemals.lsp.schema.hover.SchemaHover;
import ai.vespa.schemals.schemadocument.DocumentManager;
import ai.vespa.schemals.schemadocument.SchemaDocumentScheduler;
import ai.vespa.schemals.schemadocument.parser.schema.IdentifySymbolDefinition;
import ai.vespa.schemals.schemadocument.parser.schema.IdentifySymbolReferences;
import ai.vespa.schemals.schemadocument.resolvers.SymbolReferenceResolver;
import ai.vespa.schemals.testutils.TestLogger;
import ai.vespa.schemals.testutils.TestSchemaDiagnosticsHandler;
import ai.vespa.schemals.testutils.TestSchemaMessageHandler;
import ai.vespa.schemals.testutils.TestSchemaProgressHandler;
import ai.vespa.schemals.testutils.Utils;

/**
 * LSPTest
 */
public class LSPTest {
    /**
     * Uses a hand-crafted file to test some go-to-definition requests.
     * If this test fails, check
     * - That the file src/test/sdfiles/single/definition.sd has not been changed. 
     * - Go to definition code in {@link SchemaDefinition#getDefinition}
     * - Symbol definition logic in {@link IdentifySymbolDefinition#identify}
     * - Symbol reference logic in {@link IdentifySymbolReferences#identify}
     * - Symbol reference resolving in {@link SymbolReferenceResolver#resolveSymbolReference}
     */
    @Test
    void definitionTest() throws IOException, InvalidContextException {
        String fileName = "src/test/sdfiles/single/definition.sd";
        File file = new File(fileName);
        String fileURI = file.toURI().toString();
        String fileContent = IOUtils.readFile(file);
        TestSchemaMessageHandler messageHandler = new TestSchemaMessageHandler();
        TestSchemaProgressHandler progressHandler = new TestSchemaProgressHandler();
        ClientLogger logger = new TestLogger(messageHandler);
        SchemaIndex schemaIndex = new SchemaIndex(logger);
        TestSchemaDiagnosticsHandler diagnosticsHandler = new TestSchemaDiagnosticsHandler(new ArrayList<>());
        SchemaDocumentScheduler scheduler = new SchemaDocumentScheduler(logger, diagnosticsHandler, schemaIndex, messageHandler, progressHandler);

        scheduler.openDocument(new TextDocumentItem(fileURI, "vespaSchema", 0, fileContent));

        assertNotEquals(null, scheduler.getDocument(fileURI), "Adding a document to the scheduler should create a DocumentManager for it.");

        DocumentManager document = scheduler.getDocument(fileURI);

        // A list of tests specific to the file read above, positions are 0-indexed.
        List<Pair<Position, Range>> definitionTests = List.of(
            new Pair<>(new Position(9, 32), new Range(new Position(5, 15), new Position(5, 23))),
            new Pair<>(new Position(10, 25), new Range(new Position(6, 18), new Position(6, 19))),
            new Pair<>(new Position(17, 17), new Range(new Position(2, 14), new Position(2, 21))),
            new Pair<>(new Position(17, 25), new Range(new Position(9, 14), new Position(9, 20))),
            new Pair<>(new Position(17, 32), new Range(new Position(10, 25), new Position(10, 26))),
            new Pair<>(new Position(28, 24), new Range(new Position(21, 17), new Position(21, 20))),
            new Pair<>(new Position(28, 32), new Range(new Position(21, 17), new Position(21, 20)))
        );

        for (var testPair : definitionTests) {
            Position startPos = testPair.getFirst();
            EventPositionContext definitionContext = new EventPositionContext(
                scheduler,
                schemaIndex,
                messageHandler,
                document.getVersionedTextDocumentIdentifier(),
                startPos 
            );
            List<Location> result = SchemaDefinition.getDefinition(definitionContext);
            assertEquals(1, result.size(), "Definition request should return exactly 1 result for position " + startPos.toString());

            assertEquals(testPair.getSecond(), result.get(0).getRange(), 
                "Definition request returned wrong range for position " + startPos.toString());
        }
    }

}
